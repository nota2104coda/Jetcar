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

# Check for pymavlink module as fallback
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
  set(MAVLINK_XML_DIR "${CMAKE_SOURCE_DIR}/../mcu/include/mavlink_definitions")
  set(MAVLINK_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/mavlink")

  add_custom_command(
    OUTPUT ${MAVLINK_OUTPUT_DIR}/ardupilotmega/mavlink.h
    COMMAND ${MAVGEN_EXECUTABLE}
    ARGS --lang=C --wire-protocol=2.0 --no-validate --output ${MAVLINK_OUTPUT_DIR} ${MAVLINK_XML_DIR}/ardupilotmega.xml
    DEPENDS ${MAVLINK_XML_DIR}/ardupilotmega.xml ${MAVLINK_XML_DIR}/custom.xml ${MAVLINK_XML_DIR}/common.xml
    COMMENT "Generating MAVLink C headers from ardupilotmega.xml"
  )
  add_custom_target(generate_mavlink_headers DEPENDS ${MAVLINK_OUTPUT_DIR}/ardupilotmega/mavlink.h)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(robot_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

if(MAVGEN_EXECUTABLE AND BUILD_HW_NODES)
  add_executable(hw_mcu_node src/hw_mcu_node.cpp)
  add_dependencies(hw_mcu_node generate_mavlink_headers)
  target_include_directories(hw_mcu_node PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
  ament_target_dependencies(hw_mcu_node rclcpp std_msgs geometry_msgs robot_msgs nav_msgs sensor_msgs tf2 tf2_ros)
  install(TARGETS hw_mcu_node DESTINATION lib/${PROJECT_NAME})
endif()

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

ament_package()
