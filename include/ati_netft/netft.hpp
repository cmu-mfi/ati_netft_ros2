
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <array>
#include <string>

constexpr int PORT = 49152;
constexpr int COMMAND = 2;
constexpr int NUM_SAMPLES = 1;
constexpr int CPF = 600000;
constexpr int CPT = 1000000;

using uint32 = uint32_t;
using int32  = int32_t;
using uint16 = uint16_t;
using int16  = int16_t;
using byte   = uint8_t;

struct Response {
    uint32 rdt_sequence;
    uint32 ft_sequence;
    uint32 status;
    std::array<int32, 6> FTData;
};

class NetFT {
    public:
        NetFT(const char *ipAddress) {
            // Create socket
            socketHandle_ = socket(AF_INET, SOCK_DGRAM, 0);
            if (socketHandle_ == -1) {
                std::cerr << "Failed to create socket\n";
                exit(0);
            }

            // Build request
            *reinterpret_cast<uint16*>(&request_[0]) = htons(0x1234);
            *reinterpret_cast<uint16*>(&request_[2]) = htons(COMMAND);
            *reinterpret_cast<uint32*>(&request_[4]) = htonl(NUM_SAMPLES);

            // Resolve host
            struct hostent* he = gethostbyname(ipAddress);
            if (!he) {
                std::cerr << "Failed to resolve host\n";
                close(socketHandle_);
                exit(0);
            }

            sockaddr_in addr{};
            std::memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
            addr.sin_family = AF_INET;
            addr.sin_port = htons(PORT);

            // Connect
            if (connect(socketHandle_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
                std::cerr << "Connection failed\n";
                close(socketHandle_);
                exit(0);
            }

            std::cout << "Connected to " << ipAddress << "\n";
        };

        std::array<double, 6> getCurrentForceTorque() {
            // Send request_
            send(socketHandle_, request_.data(), request_.size(), 0);

            // Receive response
            std::array<byte, 36> response{};
            recv(socketHandle_, response.data(), response.size(), 0);

            Response resp{};
            resp.rdt_sequence = ntohl(*reinterpret_cast<uint32*>(&response[0]));
            resp.ft_sequence  = ntohl(*reinterpret_cast<uint32*>(&response[4]));
            resp.status       = ntohl(*reinterpret_cast<uint32*>(&response[8]));

            for (int i = 0; i < 6; i++) {
                resp.FTData[i] = ntohl(*reinterpret_cast<int32*>(&response[12 + i * 4]));
            }

            // Convert to real values
            std::array<double, 6> ft{};
            for (int i = 0; i < 3; i++) {
                ft[i] = static_cast<double>(resp.FTData[i]) / CPF;
            }
            for (int i = 3; i < 6; i++) {
                ft[i] = static_cast<double>(resp.FTData[i]) / CPT;
            }

            const std::array<std::string, 6> AXES = {"Fx", "Fy", "Fz", "Tx", "Ty", "Tz"};
            
            if(resp.status == 0) {
                std::cout << "Received FT data: ";
                for (int i = 0; i < 6; i++) {
                    std::cout << AXES[i] << ": " << ft[i] << " ";
                }
                std::cout << "\n";
                return ft;
            }
            else {
                std::cout << "NETFT ERROR: Status code " << std::hex << resp.status << std::dec << "\n";
                exit(0);
            }
        };

        ~NetFT() {
            close(socketHandle_);
        };

    private:
        int socketHandle_;
        std::array<byte, 8> request_{};

};