# 🧾 ROS 2 Jazzy – Master Command List (Ubuntu 24.04)

---

## 🟢 Environment & Setup

```bash
source /opt/ros/jazzy/setup.bash
source /opt/ros/jazzy/setup.zsh
printenv | grep ROS
ros2 --version
env | grep ROS
```

---

## 🟢 Workspace Navigation

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws
cd ~/ros2_ws/src
```

---

## 🟢 Build System (colcon)

```bash
colcon build
colcon build --symlink-install
colcon build --packages-select my_package
colcon build --packages-ignore my_package
colcon build --event-handlers console_direct+
colcon test
colcon test-result --verbose
rm -rf build install log
```

---

## 🟢 Workspace Sourcing

```bash
source install/setup.bash
source install/local_setup.bash
```

---

## 🟢 Package Management

```bash
ros2 pkg list
ros2 pkg prefix my_package
ros2 pkg executables my_package
ros2 pkg info my_package
```

---

## 🟢 Create Packages

### Python

```bash
ros2 pkg create my_package --build-type ament_python
ros2 pkg create my_package --build-type ament_python --dependencies rclpy std_msgs
```

### C++

```bash
ros2 pkg create my_package --build-type ament_cmake
ros2 pkg create my_package --build-type ament_cmake --dependencies rclcpp std_msgs
```

---

## 🟢 Running Nodes

```bash
ros2 run my_package my_node
ros2 run demo_nodes_cpp talker
ros2 run demo_nodes_py listener
```

---

## 🟢 Node Introspection

```bash
ros2 node list
ros2 node info /node_name
```

---

## 🟢 Topics

```bash
ros2 topic list
ros2 topic list -t
ros2 topic info /topic_name
ros2 topic info /topic_name --verbose
ros2 topic echo /topic_name
ros2 topic hz /topic_name
ros2 topic bw /topic_name
```

---

## 🟢 Publish Topics (Manual)

```bash
ros2 topic pub /topic_name std_msgs/msg/String "{data: 'hello'}"
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.5}, angular: {z: 0.1}}"
ros2 topic pub --once /topic std_msgs/msg/Bool "{data: true}"
```

---

## 🟢 Services

```bash
ros2 service list
ros2 service list -t
ros2 service type /service_name
ros2 service call /service_name std_srvs/srv/Empty
ros2 service call /service_name my_pkg/srv/MySrv "{field: value}"
```

---

## 🟢 Parameters

```bash
ros2 param list
ros2 param list /node_name
ros2 param get /node_name param_name
ros2 param set /node_name param_name value
ros2 param dump /node_name
ros2 param load /node_name params.yaml
```

---

## 🟢 Interfaces (Messages / Services / Actions)

```bash
ros2 interface list
ros2 interface list | grep msg
ros2 interface list | grep srv
ros2 interface list | grep action
ros2 interface show geometry_msgs/msg/Twist
ros2 interface show std_srvs/srv/Empty
```

---

## 🟢 Actions

```bash
ros2 action list
ros2 action list -t
ros2 action info /action_name
ros2 action send_goal /action_name my_pkg/action/MyAction "{goal_field: value}"
```

---

## 🟢 Launch Files

```bash
ros2 launch my_package my_launch.py
ros2 launch my_package my_launch.py use_sim_time:=true
ros2 launch my_package my_launch.py param1:=value
```

---

## 🟢 ROS Graph & Debug

```bash
rqt
rqt_graph
ros2 doctor
ros2 doctor --report
```

---

## 🟢 TF (Transforms)

```bash
ros2 run tf2_ros static_transform_publisher 0 0 0 0 0 0 base_link map
ros2 run tf2_tools view_frames
```

---

## 🟢 Bags (Recording & Playback)

```bash
ros2 bag record -a
ros2 bag record /topic1 /topic2
ros2 bag info bag_name
ros2 bag play bag_name
ros2 bag play bag_name --loop
```

---

## 🟢 QoS Overrides

```bash
ros2 topic echo /topic_name --qos-reliability best_effort
ros2 topic echo /topic_name --qos-durability transient_local
```

---

## 🟢 Logging

```bash
ros2 run my_package my_node --ros-args --log-level info
ros2 run my_package my_node --ros-args --log-level debug
```

---

## 🟢 Lifecycle Nodes

```bash
ros2 lifecycle list
ros2 lifecycle get /node_name
ros2 lifecycle set /node_name configure
ros2 lifecycle set /node_name activate
ros2 lifecycle set /node_name deactivate
ros2 lifecycle set /node_name shutdown
```

---

## 🟢 Daemon Control

```bash
ros2 daemon status
ros2 daemon stop
ros2 daemon start
```

---

## 🟢 Simulation / Time

```bash
ros2 param set /node_name use_sim_time true
```

---

## 🟢 Process Control

```bash
pkill -f ros2
ps aux | grep ros
```

---

## 🟢 Ubuntu Hardware & Debug (ROS-relevant)

```bash
lsusb
ls /dev/video*
dmesg | tail
ip a
htop
```

---

## 🟢 Bash Aliases (Reusable)

```bash
alias jazzy="source /opt/ros/jazzy/setup.bash"
alias cw="cd ~/ros2_ws"
alias cs="source install/setup.bash"
alias cb="colcon build --symlink-install"
alias rt="ros2 topic list"
alias rn="ros2 node list"
```

---

## 🟢 Typical Daily Sequence

```bash
source /opt/ros/jazzy/setup.bash
cd ~/ros2_ws
colcon build --symlink-install
source install/setup.bash
ros2 launch my_package my_launch.py
```