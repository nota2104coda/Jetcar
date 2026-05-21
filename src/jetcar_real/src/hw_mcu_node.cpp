#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <sensor_msgs/msg/temperature.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_ros/transform_broadcaster.h>

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
#include <map>

// MAVLink headers
// Ensure the include path is set correctly in CMakeLists.txt to point to include/
#include "mavlink/ardupilotmega/mavlink.h"

using namespace std::chrono_literals;

class McuNode : public rclcpp::Node {
public:
    McuNode() : Node("hw_mcu_node") {
        RCLCPP_INFO(this->get_logger(), "Hardware MCU node starting (USB serial + MAVLink).");

        // Parameters
        // this->declare_parameter("serial_port", "/dev/ttyUSB0");
        this->declare_parameter("serial_port", "/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_02CZJZRS-if00-port0");
        this->declare_parameter("serial_baud_rate", 921600);
        this->declare_parameter("serial_timeout", 0.01);
        this->declare_parameter("serial_chunk_size", 256); // processed in loop
        // retry mechanism handled by loop
        this->declare_parameter("poll_period", 0.01);
        this->declare_parameter("control_period", 0.05);
        this->declare_parameter("wheel_base", 0.12);
        this->declare_parameter("wheel_radius", 0.035);
        this->declare_parameter("gear_ratio", 46.0);
        this->declare_parameter("odom_frame_id", "odom");
        this->declare_parameter("base_frame_id", "base_link");
        this->declare_parameter("imu_frame_id", "base_link");
        this->declare_parameter("command_topic_manual", "/cmd_vel_manual");
        this->declare_parameter("command_topic_nav", "/cmd_vel_nav");
        this->declare_parameter("command_mode", "set_actuator_control_target");
        this->declare_parameter("stop_button_state", true);
        this->declare_parameter("auto_mode_button_state", true);
        // source/target system/component
        this->declare_parameter("command_target_system", 200);
        this->declare_parameter("command_target_component", 191);
        this->declare_parameter("command_source_system", 200);
        this->declare_parameter("command_source_component", 191);
        this->declare_parameter("manual_linear_max", 1.0);
        this->declare_parameter("manual_yaw_rate_max", 1.0);
        this->declare_parameter("manual_scale", 0.4);
        this->declare_parameter("auto_scale", 1.5);
        this->declare_parameter("flip_angular", false);
        this->declare_parameter("enable_tf_broadcast", false);

        // Get Parameters
        serial_port_ = this->get_parameter("serial_port").as_string();
        serial_baud_rate_ = this->get_parameter("serial_baud_rate").as_int();
        poll_period_ = this->get_parameter("poll_period").as_double();
        control_period_ = this->get_parameter("control_period").as_double();
        
        wheel_base_ = this->get_parameter("wheel_base").as_double();
        wheel_radius_ = this->get_parameter("wheel_radius").as_double();
        gear_ratio_ = this->get_parameter("gear_ratio").as_double();
        
        odom_frame_id_ = this->get_parameter("odom_frame_id").as_string();
        base_frame_id_ = this->get_parameter("base_frame_id").as_string();
        imu_frame_id_ = this->get_parameter("imu_frame_id").as_string();
        
        command_topic_manual_ = this->get_parameter("command_topic_manual").as_string();
        command_topic_nav_ = this->get_parameter("command_topic_nav").as_string();
        command_mode_ = this->get_parameter("command_mode").as_string();

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
        enable_tf_broadcast_ = this->get_parameter("enable_tf_broadcast").as_bool();

        // Derived calculations
        double rpm_to_rad_per_sec = (2.0 * M_PI) / 60.0;
        mps_per_rpm_ = (rpm_to_rad_per_sec * wheel_radius_) / std::max(gear_ratio_, 1e-3);

        // Publishers
        odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/mcu/odom", 10);
        imu_pub_ = this->create_publisher<sensor_msgs::msg::Imu>("/mcu/imu", 10);
        esc_pub_ = this->create_publisher<std_msgs::msg::Float32MultiArray>("/esc_telemetry", 10);
        
        range_front_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/range/front", 10);
        range_rear_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/range/rear", 10);
        cliff_front_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/cliff/front", 10);
        cliff_rear_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/cliff/rear", 10);
        // Subscribers
        cmd_vel_sub_manual_ = this->create_subscription<geometry_msgs::msg::Twist>(
            command_topic_manual_, 10, std::bind(&McuNode::manual_twist_callback, this, std::placeholders::_1));
        
        cmd_vel_sub_nav_ = this->create_subscription<geometry_msgs::msg::Twist>(
            command_topic_nav_, 10, std::bind(&McuNode::auto_twist_callback, this, std::placeholders::_1));

        stop_button_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/stop_button", 10, std::bind(&McuNode::stop_button_callback, this, std::placeholders::_1));
            
        auto_mode_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/auto_mode_button", 10, std::bind(&McuNode::auto_mode_callback, this, std::placeholders::_1));

        // TF Broadcaster
        tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

        // Open serial
        open_serial();

        // Timers
        poll_timer_ = this->create_wall_timer(
            std::chrono::duration<double>(poll_period_), std::bind(&McuNode::poll_mcu, this));
            
        control_timer_ = this->create_wall_timer(
            std::chrono::duration<double>(control_period_), std::bind(&McuNode::control_loop, this));
    }

    ~McuNode() {
        close_serial();
    }

private:
    // Serial handles
    int serial_fd_ = -1;
    std::string serial_port_;
    int serial_baud_rate_;
    
    // Config
    double poll_period_;
    double control_period_;
    double wheel_base_;
    double wheel_radius_;
    double gear_ratio_;
    std::string odom_frame_id_;
    std::string base_frame_id_;
    std::string imu_frame_id_;
    std::string command_topic_manual_;
    std::string command_topic_nav_;
    std::string command_mode_;
    
    int target_system_;
    int target_component_;
    int source_system_;
    int source_component_;
    
    double manual_linear_max_;
    double manual_yaw_rate_max_;
    double manual_scale_;
    double auto_scale_;
    bool flip_angular_;
    double mps_per_rpm_;
    bool enable_tf_broadcast_;

    // State
    bool stop_button_state_ = true;
    bool auto_mode_button_state_ = true;
    double x_ = 0.0;
    double y_ = 0.0;
    double theta_ = 0.0;
    rclcpp::Time last_time_ = this->now();
    
    // Command state
    geometry_msgs::msg::Twist last_manual_twist_;
    geometry_msgs::msg::Twist last_auto_twist_;
    rclcpp::Time last_manual_received_time_ = this->now();
    rclcpp::Time last_auto_received_time_ = this->now();

    // ROS interfaces
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr esc_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr range_front_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr range_rear_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr cliff_front_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr cliff_rear_pub_;

    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_manual_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_nav_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_button_sub_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr auto_mode_sub_;

    std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
    rclcpp::TimerBase::SharedPtr poll_timer_;
    rclcpp::TimerBase::SharedPtr control_timer_;

    // Helpers
    void open_serial() {
        if (serial_fd_ >= 0) close_serial();

        serial_fd_ = open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (serial_fd_ < 0) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open serial port %s", serial_port_.c_str());
            return;
        }

        struct termios tty;
        if (tcgetattr(serial_fd_, &tty) != 0) {
            RCLCPP_ERROR(this->get_logger(), "Error from tcgetattr");
            close_serial();
            return;
        }

        // Set baud rate
        speed_t speed;
        switch(serial_baud_rate_) {
            case 9600: speed = B9600; break;
            case 115200: speed = B115200; break;
            case 57600: speed = B57600; break;
            case 230400: speed = B230400; break;
            case 460800: speed = B460800; break;
            case 921600: speed = B921600; break;
            default: speed = B921600; 
                     RCLCPP_WARN(this->get_logger(), "Unsupported baud rate %d, using 921600", serial_baud_rate_);
                     break;
        }
        cfsetospeed(&tty, speed);
        cfsetispeed(&tty, speed);

        tty.c_cflag &= ~PARENB; // No parity
        tty.c_cflag &= ~CSTOPB; // 1 stop bit
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8; // 8 bits
        tty.c_cflag &= ~CRTSCTS; // No hardware flow control
        tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines

        tty.c_lflag &= ~ICANON; // Non-canonical mode
        tty.c_lflag &= ~ECHO; // Disable echo
        tty.c_lflag &= ~ECHOE; 
        tty.c_lflag &= ~ISIG; 

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable software flow control
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); 

        tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
        tty.c_oflag &= ~ONLCR; 

        tty.c_cc[VTIME] = 0;
        tty.c_cc[VMIN] = 0;

        if (tcsetattr(serial_fd_, TCSANOW, &tty) != 0) {
            RCLCPP_ERROR(this->get_logger(), "Error from tcsetattr");
            close_serial();
            return;
        }
        
        // Manual DTR/RTS set
        int flags;
        ioctl(serial_fd_, TIOCMGET, &flags);
        flags |= TIOCM_DTR;
        flags |= TIOCM_RTS;
        ioctl(serial_fd_, TIOCMSET, &flags);

        tcflush(serial_fd_, TCIOFLUSH);
        RCLCPP_INFO(this->get_logger(), "Serial port configured successfully.");
    }

    void close_serial() {
        if (serial_fd_ >= 0) {
            close(serial_fd_);
            serial_fd_ = -1;
        }
    }

    void poll_mcu() {
        if (serial_fd_ < 0) {
            // Try to reopen occasionally?
            static int retry_counter = 0;
            if (++retry_counter > 100) { // ~1s if 10ms poll
                open_serial();
                retry_counter = 0;
            }
            return;
        }

        uint8_t buf[256];
        int n = read(serial_fd_, buf, sizeof(buf));
        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                mavlink_message_t msg;
                mavlink_status_t status;
                if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
                    // Log message ID once every 100 messages to avoid spam
                    static int msg_count = 0;
                    if (++msg_count % 100 == 0) {
                        RCLCPP_INFO(this->get_logger(), "Received MAVLink ID: %d", msg.msgid);
                    }
                    handle_mavlink_message(msg);
                }
            }
        } else if (n < 0 && errno != EAGAIN) {
             RCLCPP_ERROR(this->get_logger(), "Serial read error: %s", strerror(errno));
             close_serial();
        }
    }

    void handle_mavlink_message(const mavlink_message_t& msg) {
        switch (msg.msgid) {
            case MAVLINK_MSG_ID_HIGHRES_IMU: {
                mavlink_highres_imu_t imu_data;
                mavlink_msg_highres_imu_decode(&msg, &imu_data);
                
                // Integrate zgyro for theta (very basic integration)
                // We don't have dt here easily unless we track last imu time
                static rclcpp::Time last_imu_time = this->now();
                rclcpp::Time now = this->now();
                double dt = (now - last_imu_time).seconds();
                if (dt > 0 && dt < 1.0) {
                     theta_ += imu_data.zgyro * dt;
                     // Wrap theta
                     while (theta_ > M_PI) theta_ -= 2*M_PI;
                     while (theta_ < -M_PI) theta_ += 2*M_PI;
                }
                // RCLCPP_INFO_THROTTLE(
                //     this->get_logger(), *this->get_clock(), 1000,  // once per second
                //     "theta = %.3f rad", theta_);
                last_imu_time = now;

                auto imu_msg = sensor_msgs::msg::Imu();
                imu_msg.header.stamp = now;
                imu_msg.header.frame_id = imu_frame_id_;
                imu_msg.linear_acceleration.x = imu_data.xacc;
                imu_msg.linear_acceleration.y = imu_data.yacc;
                imu_msg.linear_acceleration.z = imu_data.zacc;
                imu_msg.angular_velocity.z = imu_data.zgyro;

                // Covariance
                for (int i = 0; i < 9; ++i) {
                    imu_msg.orientation_covariance[i] = 0.0;
                    imu_msg.angular_velocity_covariance[i] = 0.0;
                    imu_msg.linear_acceleration_covariance[i] = 0.0;
                }
                imu_msg.angular_velocity_covariance[0] = 0.01;
                imu_msg.angular_velocity_covariance[4] = 0.01;
                imu_msg.angular_velocity_covariance[8] = 0.01;
                imu_msg.linear_acceleration_covariance[0] = 0.1;
                imu_msg.linear_acceleration_covariance[4] = 0.1;
                imu_msg.linear_acceleration_covariance[8] = 0.1;

                // Fill orientation if we trust integration, or leave empty if we only provide raw data
                // For now, let's provide orientation based on theta
                tf2::Quaternion q;
                q.setRPY(0.0, 0.0, theta_);
                imu_msg.orientation.x = q.x();
                imu_msg.orientation.y = q.y();
                imu_msg.orientation.z = q.z();
                imu_msg.orientation.w = q.w();
                imu_msg.orientation_covariance[8] = 0.05; // Z rotation covariance

                imu_pub_->publish(imu_msg);

                break;
            }
            case MAVLINK_MSG_ID_DISTANCE_SENSOR: {
                mavlink_distance_sensor_t dist;
                mavlink_msg_distance_sensor_decode(&msg, &dist);
                
                auto range_msg = sensor_msgs::msg::Range();
                range_msg.header.stamp = this->now();
                range_msg.min_range = dist.min_distance / 100.0f;
                range_msg.max_range = dist.max_distance / 100.0f;
                range_msg.range = dist.current_distance / 100.0f;
                // Field of view in rads (approx 15 deg)
                range_msg.field_of_view = 0.26; 
                
                if (dist.id == 1) {
                    range_msg.header.frame_id = "front_sonar";
                    range_msg.radiation_type = sensor_msgs::msg::Range::ULTRASOUND;
                    range_front_pub_->publish(range_msg);
                } else if (dist.id == 2) {
                    range_msg.header.frame_id = "rear_sonar";
                    range_msg.radiation_type = sensor_msgs::msg::Range::ULTRASOUND;
                    range_rear_pub_->publish(range_msg);
                } else if (dist.id == 3) {
                    range_msg.header.frame_id = "front_cliff";
                    range_msg.radiation_type = sensor_msgs::msg::Range::INFRARED;
                    cliff_front_pub_->publish(range_msg);
                } else if (dist.id == 4) {
                    range_msg.header.frame_id = "rear_cliff";
                    range_msg.radiation_type = sensor_msgs::msg::Range::INFRARED;
                    cliff_rear_pub_->publish(range_msg);
                }
                break;
            }
            case MAVLINK_MSG_ID_WHEEL_RPM: {
                mavlink_wheel_rpm_t esc;
                mavlink_msg_wheel_rpm_decode(&msg, &esc);
                float rpm[4] = {esc.rpm_fr, esc.rpm_fl, esc.rpm_rr, esc.rpm_rl}; 
                
                RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, 
                    "RPM Data: FR=%.1f, FL=%.1f, RR=%.1f, RL=%.1f", esc.rpm_fr, esc.rpm_fl, esc.rpm_rr, esc.rpm_rl);

                auto msg_arr = std_msgs::msg::Float32MultiArray();
                msg_arr.data.assign(rpm, rpm + 4);
                esc_pub_->publish(msg_arr);

                // Odometry calculation
                // Average speed (m/s)
                double avg_rpm = (esc.rpm_fr + esc.rpm_fl + esc.rpm_rr + esc.rpm_rl) / 4.0;
                double linear_vel = avg_rpm * mps_per_rpm_;
                
                // Let's update state vars
                rclcpp::Time current_time = this->now();
                double dt = (current_time - last_time_).seconds();
                if (dt > 0) {
                    double vx = linear_vel;
                    double vy = 0.0;
                     
                    double delta_x = (vx * cos(theta_) - vy * sin(theta_)) * dt;
                    double delta_y = (vx * sin(theta_) + vy * cos(theta_)) * dt;
                    
                    x_ += delta_x;
                    y_ += delta_y;
                }
                last_time_ = current_time;

                // Publish Odom
                auto odom = nav_msgs::msg::Odometry();
                odom.header.stamp = current_time;
                odom.header.frame_id = odom_frame_id_;
                odom.child_frame_id = base_frame_id_;
                odom.pose.pose.position.x = x_;
                odom.pose.pose.position.y = y_;
                odom.pose.pose.position.z = 0.0;
                
                tf2::Quaternion q;
                q.setRPY(0, 0, theta_);
                odom.pose.pose.orientation.x = q.x();
                odom.pose.pose.orientation.y = q.y();
                odom.pose.pose.orientation.z = q.z();
                odom.pose.pose.orientation.w = q.w();
                
                odom.twist.twist.linear.x = linear_vel;
                
                // Covariance
                for (int i = 0; i < 36; ++i) {
                    odom.pose.covariance[i] = 0.0;
                    odom.twist.covariance[i] = 0.0;
                }
                // Pose: x, y, z, roll, pitch, yaw
                odom.pose.covariance[0] = 0.1;  // x
                odom.pose.covariance[7] = 0.1;  // y
                odom.pose.covariance[35] = 0.2; // yaw
                // Twist: vx, vy, vz, vr, vp, vy
                odom.twist.covariance[0] = 0.05; // vx
                odom.twist.covariance[35] = 0.1; // vyaw

                odom_pub_->publish(odom);

                // TF
                if (enable_tf_broadcast_) {
                    geometry_msgs::msg::TransformStamped t;
                    t.header.stamp = current_time;
                    t.header.frame_id = odom_frame_id_;
                    t.child_frame_id = base_frame_id_;
                    t.transform.translation.x = x_;
                    t.transform.translation.y = y_;
                    t.transform.translation.z = 0.0;
                    t.transform.rotation = odom.pose.pose.orientation;
                    tf_broadcaster_->sendTransform(t);
                }
                
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
        // Decide which command to use
        rclcpp::Time now_time = this->now();
        double manual_age = (now_time - last_manual_received_time_).seconds();
        double auto_age = (now_time - last_auto_received_time_).seconds();

        double linear_x = 0.0;
        double angular_z = 0.0;

        // Timeout (2.0s)
        bool manual_active = (manual_age < 2.0);
        bool auto_active = (auto_age < 2.0);

        if (stop_button_state_) {
            // STOP
            linear_x = 0.0;
            angular_z = 0.0;
        } else if (auto_mode_button_state_) {
            // AUTO
            if (auto_active) {
                linear_x = last_auto_twist_.linear.x * auto_scale_;
                angular_z = last_auto_twist_.angular.z * auto_scale_;
            } else {
                RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "AUTO mode active but no recent cmd_vel_nav received!");
            }
        } else {
            // MANUAL
            if (manual_active) {
                linear_x = last_manual_twist_.linear.x * manual_scale_;
                angular_z = last_manual_twist_.angular.z * manual_scale_;
            }
        }

        if (std::abs(linear_x) > 0.01 || std::abs(angular_z) > 0.01) {
             RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Sending Command: v=%.2f, w=%.2f (Auto=%d)", linear_x, angular_z, auto_mode_button_state_);
        }
        
        // Clamp (though scaling 0.2 down below makes this generous)
        if (std::abs(linear_x) > manual_linear_max_) linear_x = std::copysign(manual_linear_max_, linear_x);
        if (std::abs(angular_z) > manual_yaw_rate_max_) angular_z = std::copysign(manual_yaw_rate_max_, angular_z);

        // Send MAVLink command
        send_mavlink_command(linear_x, angular_z);
    }
    
    void send_mavlink_command(double linear_x, double angular_z) {
        if (serial_fd_ < 0) return;
        
        mavlink_message_t msg;

        float vx = (float)std::max(std::min(linear_x, 1.0), -1.0);
        float wz = (float)std::max(std::min(angular_z, 1.0), -1.0);
        
        if (flip_angular_) wz = -wz;

        // Base gain
        float left = 0.8f * (vx - wz);
        float right = 0.8f * (vx + wz);
        
        // Refined deadband compensation:
        // Linearly map [0, 1] to [min_torque, 1] to preserve steering deltas
        float min_torque = 0.18f;
        auto apply_deadband = [min_torque](float val) {
            float abs_val = std::abs(val);
            if (abs_val < 0.001f) return 0.0f;
            // Map 0 -> min_torque, 1 -> 1
            float scaled = abs_val * (1.0f - min_torque) + min_torque;
            return std::copysign(scaled, val);
        };

        left = apply_deadband(left);
        right = apply_deadband(right);

        // Final clamp
        left = std::max(std::min(left, 1.0f), -1.0f);
        right = std::max(std::min(right, 1.0f), -1.0f);
        
        float controls[8] = {0.0f};
        controls[0] = right;
        controls[1] = left;
        controls[2] = right;
        controls[3] = left;
        
        uint64_t time_usec = (uint64_t)(this->now().nanoseconds() / 1000);
        
        mavlink_msg_set_actuator_control_target_pack(
            source_system_, source_component_, &msg,
            time_usec,
            0, // group_mlx (0 = default)
            target_system_, target_component_,
            controls
        );
        
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        write(serial_fd_, buf, len);
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<McuNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
