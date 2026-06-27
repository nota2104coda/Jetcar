#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/range.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>

#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

class McuNode : public rclcpp::Node {
public:
    McuNode() : Node("sim_mcu_node") {
        RCLCPP_INFO(this->get_logger(), "Simulation Virtual MCU node starting.");

        // Parameters (Identical to hw_mcu_node)
        this->declare_parameter("control_period", 0.05);
        this->declare_parameter("command_topic_manual", "/cmd_vel_manual");
        this->declare_parameter("command_topic_nav", "/cmd_vel_nav");
        this->declare_parameter("stop_button_state", true);
        this->declare_parameter("auto_mode_button_state", true);
        this->declare_parameter("manual_scale", 1.0);
        this->declare_parameter("auto_scale", 1.5);
        this->declare_parameter("flip_angular", false);

        control_period_ = this->get_parameter("control_period").as_double();
        command_topic_manual_ = this->get_parameter("command_topic_manual").as_string();
        command_topic_nav_ = this->get_parameter("command_topic_nav").as_string();
        stop_button_state_ = this->get_parameter("stop_button_state").as_bool();
        auto_mode_button_state_ = this->get_parameter("auto_mode_button_state").as_bool();
        manual_scale_ = this->get_parameter("manual_scale").as_double();
        auto_scale_ = this->get_parameter("auto_scale").as_double();

        // 1. Publishers (Identical to hw_mcu_node)
        odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/mcu/odom", 10);
        imu_pub_ = this->create_publisher<sensor_msgs::msg::Imu>("/mcu/imu", 10);
        esc_pub_ = this->create_publisher<std_msgs::msg::Float32MultiArray>("/esc_telemetry", 10);
        range_front_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/range/front", 10);
        range_rear_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/range/rear", 10);
        cliff_front_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/cliff/front", 10);
        cliff_rear_pub_ = this->create_publisher<sensor_msgs::msg::Range>("/mcu/cliff/rear", 10);

        
        
        // Sim-specific output to Gazebo
        effort_pub_ = this->create_publisher<std_msgs::msg::Float64MultiArray>("/effort_controller/commands", 10);

        // 2. Subscribers for the Gazebo Bridge (The "Virtual Sensors")
        gz_odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/gz/odom", 10, [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
                msg->header.frame_id = "odom";
                msg->child_frame_id = "base_link";
                odom_pub_->publish(*msg); // Relay to /mcu/odom
            });

        gz_imu_sub_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/gz/imu", 10, [this](const sensor_msgs::msg::Imu::SharedPtr msg) {
                msg->header.frame_id = "base_link";
                imu_pub_->publish(*msg); // Relay to /mcu/imu
            });

        gz_range_front_sub_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
            "/gz/range/front_scan", 10, [this](const sensor_msgs::msg::LaserScan::SharedPtr msg) {
                last_front_range_ = publish_range_from_scan(msg, range_front_pub_, "front_sonar");
            });

        gz_range_rear_sub_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
            "/gz/range/rear_scan", 10, [this](const sensor_msgs::msg::LaserScan::SharedPtr msg) {
                last_rear_range_ = publish_range_from_scan(msg, range_rear_pub_, "rear_sonar");
            });

        // 3. Command Subscribers (Standard)
        cmd_vel_manual_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
            command_topic_manual_, 10, std::bind(&McuNode::manual_twist_callback, this, std::placeholders::_1));
        cmd_vel_nav_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
            command_topic_nav_, 10, std::bind(&McuNode::auto_twist_callback, this, std::placeholders::_1));
        stop_button_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/stop_button", 10, std::bind(&McuNode::stop_button_callback, this, std::placeholders::_1));
        auto_mode_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/auto_mode_button", 10, std::bind(&McuNode::auto_mode_callback, this, std::placeholders::_1));

        // Control loop timer
        control_timer_ = this->create_wall_timer(std::chrono::duration<double>(control_period_), std::bind(&McuNode::control_loop, this));
    }

    ~McuNode() {} // Fixed: No close_serial here

private:
    double control_period_;
    double manual_scale_ , auto_scale_ ;
    std::string command_topic_manual_, command_topic_nav_;
    bool stop_button_state_ = true, auto_mode_button_state_ = true;
    
    geometry_msgs::msg::Twist last_manual_twist_, last_auto_twist_;
    rclcpp::Time last_manual_received_time_ = this->now(), last_auto_received_time_ = this->now();
    double last_front_range_ = 4.0 , last_rear_range_ = 4.0;
    double safety_frt_rr_range_ = 0.3;
    double safety_cliff_range_ = 0.2;

    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr range_front_pub_ , range_rear_pub_ , cliff_front_pub_ , cliff_rear_pub_;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr esc_pub_;
    rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr effort_pub_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr gz_odom_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr gz_imu_sub_;
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr gz_range_front_sub_;
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr gz_range_rear_sub_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_manual_sub_ , cmd_vel_nav_sub_ ;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_button_sub_ , auto_mode_sub_ ;

    rclcpp::TimerBase::SharedPtr control_timer_;

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

    float publish_range_from_scan(const sensor_msgs::msg::LaserScan::SharedPtr scan_msg,
                             rclcpp::Publisher<sensor_msgs::msg::Range>::SharedPtr range_pub,
                             const std::string& frame_id) {
        auto range_msg = sensor_msgs::msg::Range();
        
        // 1. Populate metadata
        range_msg.header.stamp = scan_msg->header.stamp; // Syncs timestamps (crucial for use_sim_time)
        range_msg.header.frame_id = frame_id;
        range_msg.radiation_type = sensor_msgs::msg::Range::ULTRASOUND; // Set type to Ultrasound
        
        // 2. Read physical bounds directly from the incoming scan characteristics
        range_msg.field_of_view = scan_msg->angle_max - scan_msg->angle_min;
        range_msg.min_range = scan_msg->range_min;
        range_msg.max_range = scan_msg->range_max;

        // 3. Process the rays to find the closest obstacle
        // A physical sonar sends a cone-shaped wave and reports the FIRST echo returned 
        // (i.e. the closest object within its field of view).
        float min_val = range_msg.max_range;
        bool any_valid = false;
        
        for (float r : scan_msg->ranges) {
            // Filter out non-finite values (like NaNs/infs representing clear space) 
            // and ensure the distance lies within physical sensor bounds.
            if (std::isfinite(r) && r >= range_msg.min_range && r <= range_msg.max_range) {
                if (r < min_val) {
                    min_val = r; // Keep track of the closest detected point in the cone
                    any_valid = true;
                }
            }
        }
        
        // 4. Assign range value and publish
        if (any_valid) {
            range_msg.range = min_val; // Obstacle detected
        } else {
            range_msg.range = range_msg.max_range; // Clear path, set to max range
        }

        range_pub->publish(range_msg);
        return range_msg.range;
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

        // Convert to efforts (identical math to hardware but outputs torque)
        float vx = (float)std::max(std::min(linear_x, 1.0), -1.0);
        float wz = (float)std::max(std::min(angular_z, 1.0), -1.0);
        float left = 0.8f * (vx - wz);
        float right = 0.8f * (vx + wz);

        float max_torque = 5.0f;
        auto effort_msg = std_msgs::msg::Float64MultiArray();
        effort_msg.data = {right * max_torque, left * max_torque, right * max_torque, left * max_torque};
        effort_pub_->publish(effort_msg);
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<McuNode>());
    rclcpp::shutdown();
    return 0;
}
