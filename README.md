# ATI NET F/T ROS2 Package
This packages allows users to easily integrate the ati netft sensor into their ROS2 applications and access its data in real-time.

This repository is split into two packages:
1. ati_netft - A ROS2 wrapper for the ATI NetFT force torque sensor as a regular node. 
2. netft_hardware_interface - A ros2_control harware inteface for the sensor as well as an example controller configuration.

The source code for reading the sensor data is based on [Net F/T C Sample Code](https://www.ati-ia.com/Products/ft/software/net_ft_software.aspx).

## Usage

### ati_netft package

```
ros2 run ati_netft netft --ros-args -p netft_ip:=192.168.255.255
```

```
ros2 launch ati_netft netft_launch.yaml netft_ip:=192.168.255.255
```

### netft_hardware_interface package

To start the ros2_control harware interface and force_torque_sensor_broadcaster control use the launch file:

```bash
ros2 launch netft_hardware_interface netft.launch.py
```
**Launch file arguments:**

You can configure the launch file by passing these arguments via the command line:

| Argument | Description | Default Value | Available Choices |
| :--- | :--- | :--- | :--- |
| `log_level` | The ROS logging level to use across all nodes. | `error` | `info`, `debug`, `error` |
| `ns` | Namespace of the sensor. | `""` | - |
| `netft_ip` | IP address of the sensor. | `192.168.19.210` | - |
| `cpf` | Counts per force. | `600000` | - |
| `cpt` | Counts per torque. | `1000000` | - |

**Example:**
```bash
ros2 launch netft_hardware_interface netft.launch.py log_level:=info ns:=nex10 netft_ip:=192.168.19.210
```
