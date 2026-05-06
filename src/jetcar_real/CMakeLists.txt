cmake_minimum_required(VERSION 3.8)
project(jetcar_real)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-address-of-packed-member -Wno-pedantic)
endif()

# Try to find mavgen script more robustly
find_program(MAVGEN_EXECUTABLE NAMES mavgen.py PATHS "${CMAKE_SOURCE_DIR}/../../.venv/bin" NO_DEFAULT_PATH)
if(NOT MAVGEN_EXECUTABLE)
  find_program(MAVGEN_EXECUTABLE NAMES mavgen.py)
endif()

# Check for pymavlink module as fallback if no standalone mavgen script is found
if(NOT MAVGEN_EXECUTABLE)
  execute_process(
    COMMAND python3 -c "import pymavlink.tools.mavgen; print('FOUND')"
    OUTPUT_VARIABLE PYMAVLINK_FOUND
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(PYMAVLINK_FOUND STREQUAL "FOUND")
    set(MAVGEN_EXECUTABLE python3 -m pymavlink.tools.mavgen)
  endif()
endif()

option(BUILD_HW_NODES "Build hardware nodes that depend on MAVLink" ON)

if(MAVGEN_EXECUTABLE AND BUILD_HW_NODES)
  message(STATUS "Found mavgen: ${MAVGEN_EXECUTABLE}. Building hardware nodes.")
  
  set(MAVLINK_XML_DIR "${CMAKE_SOURCE_DIR}/../mcu/include/mavlink_definitions")
  set(MAVLINK_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/mavlink")

  # Custom command to generate MAVLink headers
  add_custom_command(
    OUTPUT ${MAVLINK_OUTPUT_DIR}/ardupilotmega/mavlink.h
    COMMAND ${MAVGEN_EXECUTABLE}
    ARGS --lang=C --wire-protocol=2.0 --no-validate --output ${MAVLINK_OUTPUT_DIR} ${MAVLINK_XML_DIR}/ardupilotmega.xml
    DEPENDS ${MAVLINK_XML_DIR}/ardupilotmega.xml ${MAVLINK_XML_DIR}/custom.xml ${MAVLINK_XML_DIR}/common.xml
    COMMENT "Generating MAVLink C headers from ardupilotmega.xml"
  )

  # Custom target to ensure generation happens before compilation
  add_custom_target(generate_mavlink_headers DEPENDS ${MAVLINK_OUTPUT_DIR}/ardupilotmega/mavlink.h)
else()
  if(NOT MAVGEN_EXECUTABLE)
    message(WARNING "mavgen.py not found. Skipping hardware nodes (hw_mcu_node). Install pymavlink if you need hardware communication.")
  else()
    message(STATUS "BUILD_HW_NODES is OFF. Skipping hardware nodes.")
  endif()
endif()

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

if(MAVGEN_EXECUTABLE AND BUILD_HW_NODES)
  add_executable(hw_mcu_node src/hw_mcu_node.cpp)
  add_dependencies(hw_mcu_node generate_mavlink_headers)
  target_include_directories(hw_mcu_node PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
  ament_target_dependencies(hw_mcu_node rclcpp std_msgs geometry_msgs robot_msgs nav_msgs sensor_msgs tf2 tf2_ros)
endif()

add_executable(sim_mcu_node src/sim_mcu_node.cpp)
target_include_directories(sim_mcu_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(sim_mcu_node rclcpp std_msgs geometry_msgs nav_msgs sensor_msgs tf2 tf2_ros)

if(MAVGEN_EXECUTABLE AND BUILD_HW_NODES)
  set(HW_NODE_TARGET hw_mcu_node)
endif()

install(TARGETS hmi_node hmi_node_nobridge adaptive_resolution_node sim_mcu_node ${HW_NODE_TARGET}
  DESTINATION lib/${PROJECT_NAME})

install(PROGRAMS scripts/lidar_pwm.py
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch urdf config worlds
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
