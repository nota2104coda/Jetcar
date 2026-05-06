#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "robot_msgs/msg/button_states.hpp"

#include <chrono>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <thread>

using namespace std::chrono_literals;

class HMINode : public rclcpp::Node
{
public:
  HMINode()
  : Node("hmi_node"), last_twist_time_(this->now())
  {
    // Start Foxglove bridge
    // Launching a separate process is complex in C++ directly without libraries like boost::process
    // We'll use a simple background execution method for now, or assume it's launched via launch file
    // However, to mimic the python script exactly:
    RCLCPP_INFO(this->get_logger(), "Attempting to launch Foxglove bridge...");
    foxglove_pid_ = fork();
    if (foxglove_pid_ == 0) {
      // Child process
      // Redirect stdout/stderr to /dev/null
      freopen("/dev/null", "w", stdout);
      freopen("/dev/null", "w", stderr);
      execlp("ros2", "ros2", "launch", "foxglove_bridge", "foxglove_bridge_launch.xml", nullptr);
      exit(1); // Should not reach here
    } else if (foxglove_pid_ > 0) {
       RCLCPP_INFO(this->get_logger(), "Foxglove bridge launched with PID: %d", foxglove_pid_);
    } else {
       RCLCPP_ERROR(this->get_logger(), "Failed to fork process for Foxglove bridge");
    }

    // Initialize subscriptions
     twist_sub_manual_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel_manual", 10, std::bind(&HMINode::twist_callback, this, std::placeholders::_1));

    stop_button_sub_ = this->create_subscription<std_msgs::msg::Bool>(
      "/stop_button", 10, std::bind(&HMINode::stop_callback, this, std::placeholders::_1));

    auto_mode_sub_ = this->create_subscription<std_msgs::msg::Bool>(
      "/auto_mode_button", 10, std::bind(&HMINode::auto_mode_callback, this, std::placeholders::_1));

    // Initialize publisher
    publisher_ = this->create_publisher<robot_msgs::msg::ButtonStates>(
      "/hmi/button_states", 10);

    // Initialize timer
    timer_ = this->create_wall_timer(
      50ms, std::bind(&HMINode::publish_states, this));

    cmd_vel_timeout_ = 5.0; // seconds

    RCLCPP_INFO(this->get_logger(), "hmi_node started, publishing button states.");
  }

  ~HMINode()
  {
    if (foxglove_pid_ > 0) {
      kill(foxglove_pid_, SIGTERM);
      waitpid(foxglove_pid_, nullptr, 0);
      RCLCPP_INFO(this->get_logger(), "Foxglove bridge process terminated.");
    }
  }

private:
  void twist_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    last_twist_time_ = this->now();
    button_states_.forward = msg->linear.x > 0.1;
    button_states_.backward = msg->linear.x < -0.1;
    button_states_.left_turn = msg->angular.z > 0.1;
    button_states_.right_turn = msg->angular.z < -0.1;
    // Note: Replicating logic from python script where stop_button state is overwritten by twist callback
    button_states_.stop_button = (std::abs(msg->linear.x) < 0.1) && (std::abs(msg->linear.y) < 0.1) && (std::abs(msg->angular.z) < 0.1);
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
    // Check timeout logic (commented out in original python script, so not implementing active logic here)
    // Python code:
    // # if (time_since_last.nanoseconds / 1e9) > self.cmd_vel_timeout:
    //     # self.button_states.forward = False
    //     ...
    
    publisher_->publish(button_states_);
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr twist_sub_manual_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr stop_button_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr auto_mode_sub_;
  rclcpp::Publisher<robot_msgs::msg::ButtonStates>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;

  robot_msgs::msg::ButtonStates button_states_;
  rclcpp::Time last_twist_time_;
  double cmd_vel_timeout_;
  pid_t foxglove_pid_ = -1;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<HMINode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
