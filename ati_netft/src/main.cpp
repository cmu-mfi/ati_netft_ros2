#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/wrench_stamped.hpp"
#include "ati_netft/netft.hpp"

#define WAIT_TIME 5ms
#define DEFAULT_IP "192.168.1.1"

using namespace std::chrono_literals;

class MinimalPublisher : public rclcpp::Node
{
  public:
    MinimalPublisher()
    : Node("ft_sensor")
    {
      this->declare_parameter<std::string>("netft_ip", DEFAULT_IP);
      this->declare_parameter<int>("cpf", 600000);
      this->declare_parameter<int>("cpt", 1000000);
      std::string ip = this->get_parameter("netft_ip").as_string();
      ip_address_ = ip.c_str();
      int cpf = this->get_parameter("cpf").as_int();
      int cpt = this->get_parameter("cpt").as_int();
      netft_ = new NetFT(ip_address_, cpf, cpt);

      publisher_ = this->create_publisher<geometry_msgs::msg::WrenchStamped>("ft", 10);
      timer_ = this->create_wall_timer(
      WAIT_TIME, std::bind(&MinimalPublisher::timer_callback, this));

      RCLCPP_INFO(this->get_logger(), "Force/Torque sensor node started with IP: %s", ip_address_);
    }

  private:
    void timer_callback()
    {
      auto message = geometry_msgs::msg::WrenchStamped();

      // Get force and torque data from the sensor
      std::array<double, 6> ft_data = netft_->getCurrentForceTorque();
      message.header.stamp = this->get_clock()->now();
      message.header.frame_id = "ft_sensor_frame";
      message.wrench.force.x = ft_data[0];
      message.wrench.force.y = ft_data[1];
      message.wrench.force.z = ft_data[2];
      message.wrench.torque.x = ft_data[3];
      message.wrench.torque.y = ft_data[4];
      message.wrench.torque.z = ft_data[5];

      publisher_->publish(message);
    }
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<geometry_msgs::msg::WrenchStamped>::SharedPtr publisher_;
    NetFT *netft_;
    const char *ip_address_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MinimalPublisher>());
  rclcpp::shutdown();
  return 0;
}