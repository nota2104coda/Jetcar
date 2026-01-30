# LD06 LIDAR ROS 2 Node: Reads LD06 over UART and publishes sensor_msgs/LaserScan
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import LaserScan
import serial
import struct
import math

class LD06LidarNode(Node):
	def __init__(self):
		super().__init__('ld06_lidar_node')
		self.declare_parameter('port', '/dev/ttyUSB0')
		self.declare_parameter('baudrate', 230400)
		self.declare_parameter('topic', 'scan')
		self.port = self.get_parameter('port').get_parameter_value().string_value
		self.baudrate = self.get_parameter('baudrate').get_parameter_value().integer_value
		self.topic = self.get_parameter('topic').get_parameter_value().string_value
		self.serial = serial.Serial(self.port, self.baudrate, timeout=1)
		self.publisher = self.create_publisher(LaserScan, self.topic, 10)
		self.timer = self.create_timer(0.01, self.poll)
		self.buffer = bytearray()

	def poll(self):
		# Read available bytes
		if self.serial.in_waiting:
			self.buffer += self.serial.read(self.serial.in_waiting)
		# Try to parse packets
		while len(self.buffer) >= 47:
			if self.buffer[0] != 0x54 or self.buffer[1] != 0x2C:
				self.buffer.pop(0)
				continue
			packet = self.buffer[:47]
			if self._check_crc(packet):
				self.publish_scan(packet)
				self.buffer = self.buffer[47:]
			else:
				self.buffer.pop(0)

	def _check_crc(self, packet):
		# LD06 CRC is simple XOR of bytes 0-45, should match byte 46
		crc = 0
		for b in packet[:46]:
			crc ^= b
		return crc == packet[46]

	def publish_scan(self, packet):
		# Parse LD06 packet (see datasheet for details)
		# Packet structure: [0]=0x54, [1]=0x2C, [2]=index, [3]=speed_l, [4]=speed_h, [5:45]=12 points*3 bytes, [45]=timestamp, [46]=crc
		index = packet[2]
		speed = (packet[4] << 8 | packet[3]) / 100.0  # deg/s
		angle_start = index * 30.0
		scan = LaserScan()
		scan.header.stamp = self.get_clock().now().to_msg()
		scan.header.frame_id = 'laser'
		scan.angle_min = math.radians(angle_start)
		scan.angle_max = math.radians(angle_start + 30.0)
		scan.angle_increment = math.radians(2.5)
		scan.range_min = 0.12
		scan.range_max = 12.0
		scan.ranges = []
		scan.intensities = []
		for i in range(12):
			offset = 5 + i * 3
			dist = (packet[offset+1] << 8 | packet[offset]) / 1000.0  # meters
			intensity = packet[offset+2]
			scan.ranges.append(dist)
			scan.intensities.append(intensity)
		self.publisher.publish(scan)

def main(args=None):
	rclpy.init(args=args)
	node = LD06LidarNode()
	try:
		rclpy.spin(node)
	except KeyboardInterrupt:
		pass
	node.destroy_node()
	rclpy.shutdown()
