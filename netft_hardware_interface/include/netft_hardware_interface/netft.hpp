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
constexpr int COMMAND_STOP = 0;
constexpr int COMMAND_START = 2;

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
        NetFT(const char *ipAddress, int cpf, int cpt): cpf_(cpf), cpt_(cpt) {
            // Create socket
            socketHandle_ = socket(AF_INET, SOCK_DGRAM, 0);
            if (socketHandle_ == -1) {
                std::cerr << "Failed to create socket\n";
                exit(0);
            }

            // Add a timeout to recv() so our thread can shut down cleanly
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 100 ms timeout
            setsockopt(socketHandle_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

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

        };

        bool ping() {
            // Build request for exactly 1 sample
            std::array<byte, 8> ping_req{};
            *reinterpret_cast<uint16*>(&ping_req[0]) = htons(0x1234);
            *reinterpret_cast<uint16*>(&ping_req[2]) = htons(2); // COMMAND_START
            *reinterpret_cast<uint32*>(&ping_req[4]) = htonl(1); // NUM_SAMPLES = 1

            // Clear out any old garbage data in the receive buffer
            std::array<byte, 36> dump{};
            while(recv(socketHandle_, dump.data(), dump.size(), MSG_DONTWAIT) > 0) {}

            // Send the ping request
            send(socketHandle_, ping_req.data(), ping_req.size(), 0);

            // Wait for the single response packet (timeout is handled by SO_RCVTIMEO)
            std::array<byte, 36> response{};
            int bytes_received = recv(socketHandle_, response.data(), response.size(), 0);

            // If we got exactly 36 bytes back, the sensor is alive and talking!
            return (bytes_received == 36);
        }

        void startStreaming() {
            // Build and send request for infinite streaming (NUM_SAMPLES = 0)
            *reinterpret_cast<uint16*>(&request_[0]) = htons(0x1234);
            *reinterpret_cast<uint16*>(&request_[2]) = htons(COMMAND_START);
            *reinterpret_cast<uint32*>(&request_[4]) = htonl(0); // 0 = infinite

            send(socketHandle_, request_.data(), request_.size(), 0);
        }

        void stopStreaming() {
            // Build and send request to stop streaming
            *reinterpret_cast<uint16*>(&request_[0]) = htons(0x1234);
            *reinterpret_cast<uint16*>(&request_[2]) = htons(COMMAND_STOP);
            *reinterpret_cast<uint32*>(&request_[4]) = htonl(0);

            send(socketHandle_, request_.data(), request_.size(), 0);
        }

        // Returns true if data was received successfully, false on timeout
        bool waitForNewData(std::array<double, 6>& ft) {
            std::array<byte, 36> response{};
            int bytes_received = recv(socketHandle_, response.data(), response.size(), 0);

            if (bytes_received < 36) {
                return false; // Timeout or error
            }

            Response resp{};
            resp.rdt_sequence = ntohl(*reinterpret_cast<uint32*>(&response[0]));
            resp.ft_sequence  = ntohl(*reinterpret_cast<uint32*>(&response[4]));
            resp.status       = ntohl(*reinterpret_cast<uint32*>(&response[8]));

            if(resp.status != 0) {
                std::cerr << "NETFT ERROR: Status code " << std::hex << resp.status << std::dec << "\n";
                return false;
            }

            for (int i = 0; i < 6; i++) {
                resp.FTData[i] = ntohl(*reinterpret_cast<int32*>(&response[12 + i * 4]));
            }

            // Convert to real values
            for (int i = 0; i < 3; i++) {
                ft[i] = static_cast<double>(resp.FTData[i]) / cpf_;
            }
            for (int i = 3; i < 6; i++) {
                ft[i] = static_cast<double>(resp.FTData[i]) / cpt_;
            }

            return true;
        };

        ~NetFT() {
            stopStreaming();
            close(socketHandle_);
        };

    private:
        int socketHandle_;
        std::array<byte, 8> request_{};
        int cpf_, cpt_; // Counts per force/torque unit
};
