cmake_minimum_required(VERSION 3.8)
project(pi5_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID MATCHES "Clang")
add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclpy REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)

#Declare the custom message interface
rosidl_generate_interfaces(${PROJECT_NAME}
"msg/camera_feed.msg"
"msg/hmi_buttons.msg"
"msg/pi_command.msg"
"msg/pico_sensordata.msg"
)

install(DIRECTORY
launch
resource
test
DESTINATION share/${PROJECT_NAME}
)

#if(BUILD_TESTING)
ament_auto_add_test_dependencies()
endif()

ament_package()

install(PROGRAMS

#Install your Python nodes
pi5_pkg/camera_node.py
pi5_pkg/hmi_node.py
pi5_pkg/pico_command_node.py
pi5_pkg/pico_sensors_node.pypi
pi5_pkg/robot_state_node.py
pi5_pkg/smolvla_node.py
DESTINATION lib/${PROJECT_NAME}
)