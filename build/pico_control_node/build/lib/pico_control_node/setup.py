from setuptools import find_packages, setup

package_name = 'pico_control_node'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='your_name',
    maintainer_email='your_email@example.com',
    description='A package to control a Pico-based robot.',
    license='Apache License 2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            # This line creates the 'pico_controller' executable
            'pico_controller = pico_bot_control.pico_control_node:main',
            
            # This line creates the 'another_executable' executable
            'another_executable = pico_bot_control.another_node:main',
        ],
    },
)
