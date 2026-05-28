#ifndef NETFT_HARDWARE_INTERFACE__NETFT_HARDWARE_INTERFACE_HPP_
#define NETFT_HARDWARE_INTERFACE__NETFT_HARDWARE_INTERFACE_HPP_

#include <string>
#include <vector>

#include "hardware_interface/sensor_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "netft_hardware_interface/netft.hpp"

namespace netft_hardware_interface
{

class NetftHardwareInterface : public hardware_interface::SensorInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(NetftHardwareInterface)

  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareComponentInterfaceParams & params) override;

  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  // --- Parameters ---
  std::string netft_ip_;
  uint32_t cpf_;
  uint32_t cpt_;

  // --- States ---
  std::vector<double> ft_sensor_states_;

  // --- Hardware Driver ---
  std::unique_ptr<NetFT> netft_;

  // --- Threading & Data Buffer ---
  std::thread read_thread_;
  std::atomic<bool> is_running_{false};
  std::mutex data_mutex_;
  std::array<double, 6> latest_data_{};
  // The function that the background thread will run
  void background_read_loop();
};

}  // namespace netft_hardware_interface

#endif  // NETFT_HARDWARE_INTERFACE__NETFT_HARDWARE_INTERFACE_HPP_
