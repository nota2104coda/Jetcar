cmake_minimum_required(VERSION 3.8)
project(jetcar_common)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(robot_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rcl_interfaces REQUIRED)

add_executable(hmi_node src/hmi_node.cpp)
ament_target_dependencies(hmi_node rclcpp std_msgs geometry_msgs robot_msgs)

add_executable(hmi_node_nobridge src/hmi_node_nobridge.cpp)
ament_target_dependencies(hmi_node_nobridge rclcpp std_msgs geometry_msgs robot_msgs)

add_executable(adaptive_resolution_node src/adaptive_resolution_node.cpp)
ament_target_dependencies(adaptive_resolution_node rclcpp nav_msgs rcl_interfaces)

install(TARGETS hmi_node hmi_node_nobridge adaptive_resolution_node
  DESTINATION lib/${PROJECT_NAME})

install(PROGRAMS scripts/lidar_pwm.py
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME})
  
ament_package()
