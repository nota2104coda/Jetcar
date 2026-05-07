#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <geometry_msgs/msg/transform_stamped.hpp>

#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

class McuNode : public rclcpp::Node {
public:
    McuNode() : Node("sim_mcu_node") {
        RCLCPP_INFO(this->get_logger(), "Simulation MCU node starting.");

        // Parameters (matching hw_mcu_node)
        this->declare_parameter("control_period", 0.05);
        this->declare_parameter("command_topic_manual", "cmd_vel_manual");
        this->declare_parameter("command_topic_nav", "cmd_vel_nav");
        this->declare_parameter("manual_linear_max", 1.0);
        this->declare_parameter("manual_yaw_rate_max", 1.0);
        this->declare_parameter("manual_scale", 0.4);
        this->declare_parameter("auto_scale", 1.5);
        this->declare_parameter("stop_button_state", true);
        this->declare_parameter("auto_mode_button_state", true);

        control_period_ = this->get_parameter("control_period").as_double();
        command_topic_manual_ = this->get_parameter("command_topic_manual").as_string();
        command_topic_nav_ = this->get_parameter("command_topic_nav").as_string();
        manual_linear_max_ = this->get_parameter("manual_linear_max").as_double();
        manual_yaw_rate_max_ = this->get_parameter("manual_yaw_rate_max").as_double();
        manual_scale_ = this->get_parameter("manual_scale").as_double();
        auto_scale_ = this->get_parameter("auto_scale").as_double();
        stop_button_state_ = this->get_parameter("stop_button_state").as_bool();
        auto_mode_button_state_ = this->get_parameter("auto_mode_button_state").as_bool();

        // Publishers
        effort_pub_ = this->create_publisher<std_msgs::msg::Float64MultiArray>("/effort_controller/commands", 10);
        odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("/mcu/odom", 10);
        imu_pub_ = this->create_publisher<sensor_msgs::msg::Imu>("/mcu/imu", 10);
        scan_pub_ = this->create_publisher<sensor_msgs::msg::LaserScan>("/mcu/scan", 10);
        

        // Subscribers
        cmd_vel_sub_manual_ = this->create_subscription<geometry_msgs::msg::Twist>(
            command_topic_manual_, 10, std::bind(&McuNode::manual_twist_callback, this, std::placeholders::_1));
        
        cmd_vel_sub_nav_ = this->create_subscription<geometry_msgs::msg::Twist>(
            command_topic_nav_, 10, std::bind(&McuNode::auto_twist_callback, this, std::placeholders::_1));

        stop_button_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/stop_button", 10, std::bind(&McuNode::stop_button_callback, this, std::placeholders::_1));
            
        auto_mode_sub_ = this->create_subscription<std_msgs::msg::Bool>(
            "/auto_mode_button", 10, std::bind(&McuNode::auto_mode_callback, this, std::placeholders::_1));

        // Sensor subscribers from Gazebo (using SensorDataQoS to match Gazebo plugins)
        // Note: These must match the ROS topics published by the bridge (after remapping)
        gazebo_odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
            "/odom", rclcpp::SensorDataQoS(), std::bind(&McuNode::gazebo_odom_callback, this, std::placeholders::_1));
        gazebo_scan_sub_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
            "/scan", rclcpp::SensorDataQoS(), std::bind(&McuNode::gazebo_scan_callback, this, std::placeholders::_1));
        gazebo_imu_sub_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/imu", rclcpp::SensorDataQoS(), std::bind(&McuNode::gazebo_imu_callback, this, std::placeholders::_1));

        tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

        // Control loop timer
        control_timer_ = this->create_wall_timer(
            std::chrono::duration<double>(control_period_), std::bind(&McuNode::control_loop, this));
    }

private:
    // Config
    double control_period_;
    std::string command_topic_manual_;
    std::string command_topic_nav_;
    double manual_linear_max_;
    double manual_yaw_rate_max_;
    double manual_scale_;
    double auto_scale_;

    // State
    bool stop_button_state_ = true;
    bool auto_mode_button_state_ = true;
    
    // Command state
    geometry_msgs::msg::Twist last_manual_twist_;
    geometry_msgs::msg::Twist last_auto_twist_;
    rclcpp::Time last_manual_received_time_ = this->now();
    rclcpp::Time last_auto_received_time_ = this->now();

    // ROS interfaces
    rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr effort_pub_;
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
    rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;

    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_manual_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_nav_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_button_sub_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr auto_mode_sub_;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr gazebo_odom_sub_;
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr gazebo_scan_sub_;
    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr gazebo_imu_sub_;

    std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

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

    void gazebo_odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg) {
        RCLCPP_INFO_ONCE(this->get_logger(), "Received first odometry message from Gazebo.");
        // Republish Gazebo odom to mcu namespace
        auto odom_msg = *msg;
        odom_pub_->publish(odom_msg);
    }

    void gazebo_scan_callback(const sensor_msgs::msg::LaserScan::SharedPtr msg) {
        // Republish Gazebo scan to mcu namespace
        scan_pub_->publish(*msg);
    }

    void gazebo_imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg) {
        // Republish Gazebo imu to mcu namespace
        imu_pub_->publish(*msg);
    }

    void control_loop() {
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
             RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 1000, "Sim Sending Command: v=%.2f, w=%.2f (Auto=%d)", linear_x, angular_z, auto_mode_button_state_);
        }
        
        // Clamp
        if (std::abs(linear_x) > manual_linear_max_) linear_x = std::copysign(manual_linear_max_, linear_x);
        if (std::abs(angular_z) > manual_yaw_rate_max_) angular_z = std::copysign(manual_yaw_rate_max_, angular_z);

        // Compute efforts
        float vx = (float)std::max(std::min(linear_x, 1.0), -1.0);
        float wz = (float)std::max(std::min(angular_z, 1.0), -1.0);
        
        // Base gain
        float left = 0.8f * (vx - wz);
        float right = 0.8f * (vx + wz);
        
        // Refined deadband compensation:
        float min_torque = 0.18f;
        auto apply_deadband = [min_torque](float val) {
            float abs_val = std::abs(val);
            if (abs_val < 0.001f) return 0.0f;
            float scaled = abs_val * (1.0f - min_torque) + min_torque;
            return std::copysign(scaled, val);
        };

        left = apply_deadband(left);
        right = apply_deadband(right);

        // Final clamp (representing -1.0 to 1.0 duty cycle)
        left = std::max(std::min(left, 1.0f), -1.0f);
        right = std::max(std::min(right, 1.0f), -1.0f);

        // Scale to actual physical motor torque (100% duty cycle = 0.36 Nm)
        float max_torque_nm = 0.36f;
        double left_torque = left * max_torque_nm;
        double right_torque = right * max_torque_nm;

        // Publish efforts to Gazebo
        // Order must match joints in yaml: fr, fl, rr, rl
        auto effort_msg = std_msgs::msg::Float64MultiArray();
        effort_msg.data = {right_torque, left_torque, right_torque, left_torque};
        effort_pub_->publish(effort_msg);
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<McuNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
