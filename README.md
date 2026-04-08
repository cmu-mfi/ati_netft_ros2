# ATI NET F/T ROS2 Package

This package provides a ROS2 wrapper for the ATI NetFT force torque sensor. It allows users to easily integrate the sensor into their ROS2 applications and access its data in real-time.

The source code for reading the sensor data is based on [Net F/T C Sample Code](https://www.ati-ia.com/Products/ft/software/net_ft_software.aspx).

## Usage

```
ros2 run ati_netft netft --ros-args -p netft_ip:=192.168.255.255
```

```
ros2 launch ati_netft netft_launch.yaml netft_ip:=192.168.255.255
```