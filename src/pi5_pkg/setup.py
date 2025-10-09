from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'pi5_pkg'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),        
        (os.path.join('share', package_name, 'launch'), glob('launch/*launch.[pxy][yma]*')),
        (os.path.join('share', package_name, 'urdf'), glob('urdf/**/*', recursive=True)),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='nota2104coda',
    maintainer_email='jeevanghadge@gmail.com',
    description='TODO: Package description',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            "camera_node = pi5_pkg.camera_node:main",
            'pico_sensors_node = pi5_pkg.pico_sensors_node:main',
            'hmi_node = pi5_pkg.hmi_node:main'
        ],
    },
)
