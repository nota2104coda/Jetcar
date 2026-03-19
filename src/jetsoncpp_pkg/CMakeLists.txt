cmake_minimum_required(VERSION 3.8)
project(jetsoncpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-address-of-packed-member -Wno-pedantic)
endif()

# Define paths
set(MAVGEN_SCRIPT "${CMAKE_SOURCE_DIR}/../../.venv/bin/mavgen.py")
set(MAVLINK_XML_DIR "${CMAKE_SOURCE_DIR}/../mcu/include/mavlink_definitions")
set(MAVLINK_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/mavlink")

# Custom command to generate MAVLink headers
add_custom_command(
  OUTPUT ${MAVLINK_OUTPUT_DIR}/ardupilotmega/mavlink.h
  COMMAND ${MAVGEN_SCRIPT}
  ARGS --lang=C --wire-protocol=2.0 --no-validate --output ${MAVLINK_OUTPUT_DIR} ${MAVLINK_XML_DIR}/ardupilotmega.xml
  DEPENDS ${MAVLINK_XML_DIR}/ardupilotmega.xml ${MAVLINK_XML_DIR}/custom.xml ${MAVLINK_XML_DIR}/common.xml
  COMMENT "Generating MAVLink C headers from ardupilotmega.xml"
)

# Custom target to ensure generation happens before compilation
add_custom_target(generate_mavlink_headers DEPENDS ${MAVLINK_OUTPUT_DIR}/ardupilotmega/mavlink.h)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(robot_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(rcl_interfaces REQUIRED)


add_executable(hmi_node src/hmi_node.cpp)
target_include_directories(hmi_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(hmi_node rclcpp std_msgs geometry_msgs robot_msgs)

add_executable(hmi_node_nobridge src/hmi_node_nobridge.cpp)
target_include_directories(hmi_node_nobridge PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(hmi_node_nobridge rclcpp std_msgs geometry_msgs robot_msgs)

add_executable(adaptive_resolution_node src/adaptive_resolution_node.cpp)
target_include_directories(adaptive_resolution_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(adaptive_resolution_node rclcpp nav_msgs rcl_interfaces)

add_executable(hw_mcu_node src/hw_mcu_node.cpp)
add_dependencies(hw_mcu_node generate_mavlink_headers)
target_include_directories(hw_mcu_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(hw_mcu_node rclcpp std_msgs geometry_msgs robot_msgs nav_msgs sensor_msgs tf2 tf2_ros)

install(TARGETS hmi_node hmi_node_nobridge adaptive_resolution_node hw_mcu_node
  DESTINATION lib/${PROJECT_NAME})

install(PROGRAMS scripts/lidar_pwm.py
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch urdf config
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
