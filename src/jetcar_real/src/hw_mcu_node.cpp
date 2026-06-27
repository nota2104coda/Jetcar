#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>
// #include <tf2/LinearMath/Quaternion.h>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

#include <chrono>
#include <cmath>
#include <thread>
#include <vector>
#include <string>

// MAVLink headers
#include "mavlink/ardupilotmega/mavlink.h"

using namespace std::chrono_literals;

class McuNode : public rclcpp::Node {
public:
    McuNode() : Node("hw_mcu_node") {
        RCLCPP_INFO(this->get_logger(), "Hardware MCU node starting (USB serial + MAVLink).");

        this->declare_parameter("serial_port", "/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_02CZJZRS-if00-port0");
        this->declare_parameter("serial_baud_rate", 921600);
        this->declare_parameter("poll_period", 0.005);
        this->declare_parameter("control_period", 0.05);
        this->declare_parameter("wheel_radius", 0.035);
        this->declare_parameter("gear_ratio", 46.0);
        this->declare_parameter("odom_frame_id", "odom");
        this->declare_parameter("base_frame_id", "base_link");
        this->declare_parameter("imu_frame_id", "base_link");
        this->declare_parameter("command_topic_manual", "/cmd_vel_manual");
        this->declare_parameter("command_topic_nav", "/cmd_vel_nav");
        this->declare_parameter("stop_button_state", true);
        this->declare_parameter("auto_mode_button_state", true);
        this->declare_parameter("command_target_system", 200);
        this->declare_parameter("command_target_component", 191);
        this->declare_parameter("command_source_system", 200);
        this->declare_parameter("command_source_component", 191);
        this->declare_parameter("manual_linear_max", 1.0);
        this->declare_parameter("manual_yaw_rate_max", 1.0);
        this->declare_parameter("manual_scale", 0.4);
        this->declare_parameter("auto_scale", 1.5);
        this->declare_parameter("flip_angular", false);

        serial_port_ = this->get_parameter("serial_port").as_string();
        serial_baud_rate_ = this->get_parameter("serial_baud_rate").as_int();
        poll_period_ = this->get_parameter("poll_period").as_double();
        control_period_ = this->get_parameter("control_period").as_double();
        wheel_radius_ = this->get_parameter("wheel_radius").as_double();
        gear_ratio_ = this->get_parameter("gear_ratio").as_double();
        odom_frame_id_ = this->get_parameter("odom_frame_id").as_string();
        base_frame_id_ = this->get_parameter("base_frame_id").as_string();
        imu_frame_id_ = this->get_parameter("imu_frame_id").as_string();
        command_topic_manual_ = this->get_parameter("command_topic_manual").as_string();
        command_topic_nav_ = this->get_parameter("command_topic_nav").as_string();
        stop_button_state_ = this->get_parameter("stop_button_state").as_bool();
        auto_mode_button_state_ = this->get_parameter("auto_mode_button_state").as_bool();
        target_system_ = this->get_parameter("command_target_system").as_int();
        target_component_ = this->get_parameter("command_target_component").as_int();
        source_system_ = this->get_parameter("command_source_system").as_int();
        source_component_ = this->get_parameter("command_source_component").as_int();
        manual_linear_max_ = this->get_parameter("manual_linear_max").as_double();
        manual_yaw_rate_max_ = this->get_parameter("manual_yaw_rate_max").as_double();
        manual_scale_ = this->get_parameter("manual_scale").as_double();
        auto_scale_ = this->get_parameter("auto_scale").as_double();
        flip_angular_ = this->get_parameter("flip_angular").as_bool();

        double rpm_to_rad_per_sec = (2.0 * M_PI) / 60.0;
        mps_per_rpm_ = (rpm_to_rad_per_sec * wheel_radius_) / std::max(gear_ratio_, 1e-3);

        // 1. Publishers (Identical to hw_mcu_node)
        odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/mcu/odom", 10);
        imu_pub_ = this->create_publisher<sensor_msgs::msg::Imu>("/mcu/imu", 10);
        esc_pub_ = this->create_publisher<std_msgs::msg::Float32MultiArray>("/esc_telemetry", 10);
        range_front_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/range/front", 10);
        range_rear_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/range/rear", 10);
        cliff_front_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/cliff/front", 10);
        cliff_rear_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/cliff/rear", 10);

        cmd_vel_manual_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
            command_topic_manual_, 10, std::bind(&McuNode::manual_twist_callback, this, std::placeholders::_1));
        cmd_vel_nav_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
            command_topic_nav_, 10, std::bind(&McuNode::auto_twist_callback, this, std::placeholders::_1));
        stop_button_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/stop_button", 10, std::bind(&McuNode::stop_button_callback, this, std::placeholders::_1));
        auto_mode_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/auto_mode_button", 10, std::bind(&McuNode::auto_mode_callback, this, std::placeholders::_1));

        open_serial();

        poll_timer_ = this->create_wall_timer(std::chrono::duration<double>(poll_period_), std::bind(&McuNode::poll_mcu, this));
        control_timer_ = this->create_wall_timer(std::chrono::duration<double>(control_period_), std::bind(&McuNode::control_loop, this));
    }

    ~McuNode() { close_serial(); }

private:
    int serial_fd_ = -1;
    std::string serial_port_;
    int serial_baud_rate_;
    double control_period_;
    double manual_scale_ , auto_scale_ ;
    double poll_period_, wheel_radius_, gear_ratio_;
    std::string odom_frame_id_, base_frame_id_, imu_frame_id_;
    std::string command_topic_manual_, command_topic_nav_;
    int target_system_, target_component_, source_system_, source_component_;
    double manual_linear_max_, manual_yaw_rate_max_ ;
    
    bool flip_angular_;
    double mps_per_rpm_;
    bool stop_button_state_ = true, auto_mode_button_state_ = true;
    double x_ = 0.0, y_ = 0.0, theta_ = 0.0;
    rclcpp::Time last_time_ = this->now();

    geometry_msgs::msg::Twist last_manual_twist_, last_auto_twist_;
    rclcpp::Time last_manual_received_time_ = this->now(), last_auto_received_time_ = this->now();
    double last_front_range_ = 4.0 , last_rear_range_ = 4.0;
    double safety_frt_rr_range_ = 0.3;
    double safety_cliff_range_ = 0.2;

    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr range_front_pub_ , range_rear_pub_ , cliff_front_pub_ , cliff_rear_pub_;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr esc_pub_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_manual_sub_ , cmd_vel_nav_sub_ ;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_button_sub_ , auto_mode_sub_ ;
    rclcpp::TimerBase::SharedPtr poll_timer_, control_timer_;

    void open_serial() {
        if (serial_fd_ >= 0) close_serial();
        serial_fd_ = open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (serial_fd_ < 0) return;
        struct termios tty;
        tcgetattr(serial_fd_, &tty);
        cfsetospeed(&tty, B921600); cfsetispeed(&tty, B921600);
        tty.c_cflag &= ~PARENB; tty.c_cflag &= ~CSTOPB; tty.c_cflag &= ~CSIZE; tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS; tty.c_cflag |= CREAD | CLOCAL;
        tty.c_lflag &= ~ICANON; tty.c_lflag &= ~ECHO; tty.c_lflag &= ~ECHOE; tty.c_lflag &= ~ISIG;
        tty.c_iflag &= ~(IXON | IXOFF | IXANY); tty.c_oflag &= ~OPOST;
        tty.c_cc[VTIME] = 0; tty.c_cc[VMIN] = 0;
        tcsetattr(serial_fd_, TCSANOW, &tty);
        tcflush(serial_fd_, TCIOFLUSH);
    }

    void close_serial() { if (serial_fd_ >= 0) { close(serial_fd_); serial_fd_ = -1; } }

    void poll_mcu() {
        if (serial_fd_ < 0) return;
        uint8_t buf[256];
        int n = read(serial_fd_, buf, sizeof(buf));
        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                mavlink_message_t msg;
                mavlink_status_t status;
                if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) handle_mavlink_message(msg);
            }
        }
    }

    void handle_mavlink_message(const mavlink_message_t& msg) {
        switch (msg.msgid) {
            case MAVLINK_MSG_ID_HIGHRES_IMU: {
                mavlink_highres_imu_t imu_data;
                mavlink_msg_highres_imu_decode(&msg, &imu_data);
                auto imu_msg = sensor_msgs::msg::Imu();
                imu_msg.header.stamp = this->now();
                imu_msg.header.frame_id = imu_frame_id_;
                imu_msg.linear_acceleration.x = imu_data.xacc;
                imu_msg.linear_acceleration.y = imu_data.yacc;
                imu_msg.linear_acceleration.z = imu_data.zacc;
                imu_msg.angular_velocity.z = imu_data.zgyro;
                imu_pub_->publish(imu_msg);
                break;
            }
            case MAVLINK_MSG_ID_WHEEL_RPM: {
                mavlink_wheel_rpm_t esc;
                mavlink_msg_wheel_rpm_decode(&msg, &esc);
                double avg_rpm = (esc.rpm_fr + esc.rpm_fl + esc.rpm_rr + esc.rpm_rl) / 4.0;
                double linear_vel = avg_rpm * mps_per_rpm_;
                auto odom = nav_msgs::msg::Odometry();
                odom.header.stamp = this->now();
                odom.header.frame_id = odom_frame_id_;
                odom.child_frame_id = base_frame_id_;
                odom.twist.twist.linear.x = linear_vel;
                odom_pub_->publish(odom);
                break;
            }
        }
    }

    void manual_twist_callback(const geometry_msgs::msg::Twist::SharedPtr msg) {
        last_manual_twist_ = *msg;
        last_manual_received_time_ = this->now();
    }
    void auto_twist_callback(const geometry_msgs::msg::Twist::SharedPtr msg) {
        last_auto_twist_ = *msg;
        last_auto_received_time_ = this->now();
    }
    void stop_button_callback(const std_msgs::msg::Bool::SharedPtr msg) {
        stop_button_state_ = msg->data;
        if (stop_button_state_) {
            last_manual_twist_ = geometry_msgs::msg::Twist();
            last_auto_twist_ = geometry_msgs::msg::Twist();
        }
    }
    void auto_mode_callback(const std_msgs::msg::Bool::SharedPtr msg) {
        auto_mode_button_state_ = msg->data;
        if (!auto_mode_button_state_) {
            last_auto_twist_ = geometry_msgs::msg::Twist();
        }
    }

    void control_loop() {
        double linear_x = 0.0, angular_z = 0.0;
        //set timeout to 1.0sec, same in sim_mcu_node and hw_mcu_node
        if (!stop_button_state_) {
            if (auto_mode_button_state_ && (this->now() - last_auto_received_time_).seconds() < 1.0) {
                linear_x = last_auto_twist_.linear.x * auto_scale_;
                angular_z = last_auto_twist_.angular.z * auto_scale_;
            } else if (!auto_mode_button_state_ && (this->now() - last_manual_received_time_).seconds() < 1.0) {
                linear_x = last_manual_twist_.linear.x * manual_scale_;
                angular_z = last_manual_twist_.angular.z * manual_scale_;
            }
        }   

        // Safety override: if obstacle is closer than 30cm (0.3m) in front, prevent forward motion
        if (linear_x > 0.0 && last_front_range_ <= safety_frt_rr_range_ ) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                "Forward obstacle detected (Range: %.2fm <= safety distance %.2fm). Blocking forward motion.", last_front_range_, safety_frt_rr_range_);
            linear_x = 0.0;
        } else if (linear_x < 0 && last_rear_range_ <= safety_frt_rr_range_ ) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                "Rear obstacle detected (Range: %.2fm <= safety distance %.2fm). Blocking backward motion.", last_rear_range_, safety_frt_rr_range_);
            linear_x = 0.0;
        }
        
        send_mavlink_command(linear_x, angular_z);
    }

    void send_mavlink_command(double linear_x, double angular_z) {
        if (serial_fd_ < 0) return;
        mavlink_message_t msg;
        float vx = (float)std::max(std::min(linear_x, 1.0), -1.0);
        float wz = (float)std::max(std::min(angular_z, 1.0), -1.0);
        if (flip_angular_) wz = -wz;
        float left = 0.8f * (vx - wz);
        float right = 0.8f * (vx + wz);
        float controls[8] = {right, left, right, left, 0, 0, 0, 0};
        mavlink_msg_set_actuator_control_target_pack(source_system_, source_component_, &msg, this->now().nanoseconds()/1000, 0, target_system_, target_component_, controls);
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        write(serial_fd_, buf, len);
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<McuNode>());
    rclcpp::shutdown();
    return 0;
}
