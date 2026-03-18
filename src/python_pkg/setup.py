from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'python_pkg'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),        
        (os.path.join('share', package_name, 'launch'), glob('launch/*launch.[pxy][yma]*')),
        (os.path.join('share', package_name, 'urdf'), [
            path for path in glob('urdf/**/*', recursive=True)
            if os.path.isfile(path)
        ]),
    ],
    install_requires=['setuptools', 'pyserial','smbus2'],
    zip_safe=True,
    maintainer='nota2104coda',
    maintainer_email='jeevanghadge@gmail.com',
    description='TODO: Package description',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            "camera_node = python_pkg.camera_node:main",
            "adaptive_resolution_node = python_pkg.adaptive_resolution_node:main",
            'hmi_node = python_pkg.hmi_node:main',
            'hw_mcu_node = python_pkg.hw_mcu_node:main',
            'ld06_lidar_node = python_pkg.ld06_lidar_node:main',
            # 'ldlidar_node = python_pkg.ldlidar_node:main',
            'hw_realsense_node = python_pkg.hw_realsense_node:main'
        ],
    },
)
