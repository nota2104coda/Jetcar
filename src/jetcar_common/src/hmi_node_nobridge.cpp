#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "robot_msgs/msg/button_states.hpp"

#include <chrono>
#include <string>

using namespace std::chrono_literals;

class HMINodeNoBridge : public rclcpp::Node
{
public:
  HMINodeNoBridge()
  : Node("hmi_node"), last_twist_time_(this->now())
  {
    // Initialize subscriptions
    twist_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel_manual", 10, std::bind(&HMINodeNoBridge::twist_callback, this, std::placeholders::_1));
    
    stop_button_sub_ = this->create_subscription<std_msgs::msg::Bool>(
      "/stop_button", 10, std::bind(&HMINodeNoBridge::stop_callback, this, std::placeholders::_1));

    auto_mode_sub_ = this->create_subscription<std_msgs::msg::Bool>(
      "/auto_mode_button", 10, std::bind(&HMINodeNoBridge::auto_mode_callback, this, std::placeholders::_1));

    // Initialize publisher
    publisher_ = this->create_publisher<robot_msgs::msg::ButtonStates>(
      "/hmi/button_states", 10);

    // Initialize timer
    timer_ = this->create_wall_timer(
      50ms, std::bind(&HMINodeNoBridge::publish_states, this));

    cmd_vel_timeout_ = 5.0; // seconds

    RCLCPP_INFO(this->get_logger(), "hmi_node (no bridge) started, publishing button states.");
  }

  ~HMINodeNoBridge()
  {
  }

private:
  void twist_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    last_twist_time_ = this->now();
    button_states_.forward = msg->linear.x > 0.1;
    button_states_.backward = msg->linear.x < -0.1;
    button_states_.left_turn = msg->angular.z > 0.1;
    button_states_.right_turn = msg->angular.z < -0.1;
  }

  void stop_callback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    button_states_.stop_button = msg->data;
  }

  void auto_mode_callback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    button_states_.auto_mode_button = msg->data;
  }

  void publish_states()
  {
    publisher_->publish(button_states_);
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr twist_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_button_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr auto_mode_sub_;
  rclcpp::Publisher<robot_msgs::msg::ButtonStates>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;

  robot_msgs::msg::ButtonStates button_states_;
  rclcpp::Time last_twist_time_;
  double cmd_vel_timeout_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<HMINodeNoBridge>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
