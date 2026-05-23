#include "netft_hardware_interface/netft_hardware_interface.hpp"
#include "pluginlib/class_list_macros.hpp"

namespace netft_hardware_interface
{
  hardware_interface::CallbackReturn NetftHardwareInterface::on_init(const hardware_interface::HardwareComponentInterfaceParams & params) {
    // Note: Change to SensorInterface
    if (hardware_interface::SensorInterface::on_init(params) != hardware_interface::CallbackReturn::SUCCESS) {
      return hardware_interface::CallbackReturn::ERROR;
    }

    // 1. Load Parameters
    netft_ip_ = info_.hardware_parameters["netft_ip"];
    if (netft_ip_.empty()) {
      RCLCPP_ERROR(rclcpp::get_logger("NetftHardwareInterface"), "[INIT] Ip address not provided!");
      return hardware_interface::CallbackReturn::ERROR;
    }
    try {
      cpf_ = std::stoul(info_.hardware_parameters.at("cpf"));
    } catch (const std::exception& e) {
      RCLCPP_ERROR(rclcpp::get_logger("NetftHardwareInterface"), "[INIT] 'cpf' not provided or invalid!");
      return hardware_interface::CallbackReturn::ERROR;
    }
    try {
      cpt_ = std::stoul(info_.hardware_parameters.at("cpt"));
    } catch (const std::exception& e) {
      RCLCPP_ERROR(rclcpp::get_logger("NetftHardwareInterface"), "[INIT] 'cpt' not provided or invalid!");
      return hardware_interface::CallbackReturn::ERROR;
    }
    // 2. Initialize State Variables
    ft_sensor_states_.assign(6, std::numeric_limits<double>::quiet_NaN());

    RCLCPP_INFO(rclcpp::get_logger("NetftHardwareInterface"), 
                "[INIT] Successfully initialized. IP: %s, CPF: %u, CPT: %u", 
                netft_ip_.c_str(), cpf_, cpt_);

    return hardware_interface::CallbackReturn::SUCCESS;
  }

  hardware_interface::CallbackReturn NetftHardwareInterface::on_configure(const rclcpp_lifecycle::State & /*previous_state*/) {
    RCLCPP_INFO(rclcpp::get_logger("NetftHardwareInterface"), "[CONFIG] Connecting to %s ", netft_ip_.c_str());

    try {
      netft_ = std::make_unique<NetFT>(netft_ip_.c_str(), cpf_, cpt_);
      
      // Ping the sensor to verify physical reachability
      if (!netft_->ping()) {
        RCLCPP_ERROR(rclcpp::get_logger("NetftHardwareInterface"), 
                     "[CONFIG] Failed to ping sensor at %s. Check power, ethernet connection, and IP address.", 
                     netft_ip_.c_str());
        return hardware_interface::CallbackReturn::FAILURE;
      }

    } catch (const std::exception& e) {
      RCLCPP_ERROR(rclcpp::get_logger("NetftHardwareInterface"), "[CONFIG] Failed to initialize NetFT: %s", e.what());
      return hardware_interface::CallbackReturn::ERROR;
    }

    RCLCPP_INFO(rclcpp::get_logger("NetftHardwareInterface"), "[CONFIG] Connection established and sensor verified!");
    return hardware_interface::CallbackReturn::SUCCESS;
  }

  hardware_interface::CallbackReturn NetftHardwareInterface::on_activate(const rclcpp_lifecycle::State & /*previous_state*/) {
    RCLCPP_INFO(rclcpp::get_logger("NetftHardwareInterface"), "[ACTIVATION] Activating Sensor and starting stream!");
    // 1. Tell the hardware to start blasting UDP packets
    netft_->startStreaming();
    // 2. Initialize our background buffer to NaNs
    latest_data_.fill(std::numeric_limits<double>::quiet_NaN());
    // 3. Start the background thread
    is_running_ = true;
    read_thread_ = std::thread(&NetftHardwareInterface::background_read_loop, this);
    RCLCPP_INFO(rclcpp::get_logger("NetftHardwareInterface"), "[ACTIVATION] Activated successfully!");
    return hardware_interface::CallbackReturn::SUCCESS;
  }

  hardware_interface::CallbackReturn NetftHardwareInterface::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/) {
    RCLCPP_INFO(rclcpp::get_logger("NetftHardwareInterface"), "[DEACTIVATION] Deactivating Sensor and stopping stream!");
    // 1. Tell the background thread to stop
    is_running_ = false;
    // 2. Wait for the thread to finish cleanly (max wait is 100ms due to our socket timeout)
    if (read_thread_.joinable()) {
      read_thread_.join();
    }
    // 3. Tell the hardware to stop sending UDP packets
    netft_->stopStreaming();
    RCLCPP_INFO(rclcpp::get_logger("NetftHardwareInterface"), "[DEACTIVATION] Deactivated successfully!");
    return hardware_interface::CallbackReturn::SUCCESS;
  }

  void NetftHardwareInterface::background_read_loop() {
    std::array<double, 6> ft_data;
    // Loop until on_deactivate sets is_running_ to false
    while (is_running_) {
      // waitForNewData blocks until a packet arrives, or the 100ms socket timeout is hit
      if (netft_->waitForNewData(ft_data)) {
        // Data arrived! Safely copy it into our shared variable
        std::lock_guard<std::mutex> lock(data_mutex_);
        latest_data_ = ft_data;
      }
    }
  }

  hardware_interface::return_type NetftHardwareInterface::read(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/) {
    
    // Safely copy the latest data from the background thread into the ros2_control states array
    std::lock_guard<std::mutex> lock(data_mutex_);
    for (size_t i = 0; i < 6; ++i) {
      ft_sensor_states_[i] = latest_data_[i];
    }

    return hardware_interface::return_type::OK;
  }

  std::vector<hardware_interface::StateInterface> NetftHardwareInterface::export_state_interfaces() {
    std::vector<hardware_interface::StateInterface> state_interfaces;
    if (info_.sensors.empty()) {
      RCLCPP_ERROR(rclcpp::get_logger("NetftHardwareInterface"), "No sensors defined in the URDF!");
      return state_interfaces;
    }
    std::string sensor_name = info_.sensors[0].name;
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      sensor_name, "force.x", &ft_sensor_states_[0]));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      sensor_name, "force.y", &ft_sensor_states_[1]));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      sensor_name, "force.z", &ft_sensor_states_[2]));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      sensor_name, "torque.x", &ft_sensor_states_[3]));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      sensor_name, "torque.y", &ft_sensor_states_[4]));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      sensor_name, "torque.z", &ft_sensor_states_[5]));

    return state_interfaces;
  }

}  // namespace netft_hardware_interface

// Export the class to pluginlib so it can be dynamically loaded
// Note: Base class changed to hardware_interface::SensorInterface
PLUGINLIB_EXPORT_CLASS(
    netft_hardware_interface::NetftHardwareInterface, hardware_interface::SensorInterface)
