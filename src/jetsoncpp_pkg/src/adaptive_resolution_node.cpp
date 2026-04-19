#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "rcl_interfaces/srv/set_parameters.hpp"
#include "rcl_interfaces/msg/parameter.hpp"
#include "rcl_interfaces/msg/parameter_value.hpp"
#include "rcl_interfaces/msg/parameter_type.hpp"

#include <cmath>
#include <chrono>

using namespace std::chrono_literals;

class AdaptiveResolutionNode : public rclcpp::Node
{
public:
  AdaptiveResolutionNode()
  : Node("adaptive_resolution_node")
  {
    // Parameters
    this->declare_parameter("low_speed_threshold", 0.2);
    this->declare_parameter("high_speed_threshold", 0.3);
    this->declare_parameter("enable_dynamic_decimation", true);
    this->declare_parameter("camera_node_name", "/camera/camera");

    low_speed_limit_ = this->get_parameter("low_speed_threshold").as_double();
    high_speed_limit_ = this->get_parameter("high_speed_threshold").as_double();
    camera_node_name_ = this->get_parameter("camera_node_name").as_string();

    current_state_ = HIGH_RES;

    // Subscriber
    odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
      "/odom", 10, std::bind(&AdaptiveResolutionNode::odom_callback, this, std::placeholders::_1));

    // Service Client
    std::string service_name = camera_node_name_ + "/set_parameters";
    param_client_ = this->create_client<rcl_interfaces::srv::SetParameters>(service_name);

    RCLCPP_INFO(this->get_logger(), "Adaptive Resolution Node Started");
    RCLCPP_INFO(this->get_logger(), "Waiting for %s service...", service_name.c_str());
    
    // We won't block here with wait_for_service inside constructor to avoid blocking the node initialization
    // The service check will happen before calling it.
  }

private:
  enum State {
    HIGH_RES,
    LOW_RES
  };

  void odom_callback(const nav_msgs::msg::Odometry::SharedPtr msg)
  {
    if (!this->get_parameter("enable_dynamic_decimation").as_bool()) {
      return;
    }

    double vx = msg->twist.twist.linear.x;
    double vy = msg->twist.twist.linear.y;
    double speed = std::sqrt(vx * vx + vy * vy);

    if (current_state_ == HIGH_RES && speed > high_speed_limit_) {
      set_decimation(2); // Low Resolution
      current_state_ = LOW_RES;
      // RCLCPP_INFO(this->get_logger(), "Speed %.2f > %.2f: Switching to Low Res (Decimation 2)", speed, high_speed_limit_);
    } else if (current_state_ == LOW_RES && speed < low_speed_limit_) {
      set_decimation(1); // High Resolution
      current_state_ = HIGH_RES;
      // RCLCPP_INFO(this->get_logger(), "Speed %.2f < %.2f: Switching to High Res (Decimation 1)", speed, low_speed_limit_);
    }
  }

  void set_decimation(int magnitude)
  {
    if (!param_client_->service_is_ready()) {
      // Just return silently to avoid log spam, mirroring python behavior which checks is_ready
      return;
    }

    auto request = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
    rcl_interfaces::msg::Parameter param;
    param.name = "decimation_filter.filter_magnitude";
    param.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER;
    param.value.integer_value = magnitude;
    request->parameters.push_back(param);
    
    // Asynchronous call
    param_client_->async_send_request(request);
  }

  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Client<rcl_interfaces::srv::SetParameters>::SharedPtr param_client_;
  
  double low_speed_limit_;
  double high_speed_limit_;
  std::string camera_node_name_;
  State current_state_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<AdaptiveResolutionNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
