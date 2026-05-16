#include "gateway.hpp"
#include "router.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
#include "iothub.h"
#include "iothubtransportmqtt.h"
#include "iothub_device_client_ll.h"
#include "iothub_message.h"

static std::atomic<bool> g_run{true};

static IOTHUB_DEVICE_CLIENT_LL_HANDLE g_iotHubClient = nullptr;
static std::atomic<bool> g_iotHubInitialized{false};

static std::string gateway_intersection_id()
{
    const char* env = std::getenv("GATEWAY_INTERSECTION_ID");
    return (env && env[0]) ? std::string(env) : "gateway-1";
}

static void iothub_send_confirmation_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void*)
{
    std::cout << "Azure IoT Hub send confirmation: " << result << std::endl;
}

static bool init_iot_hub_client()
{
    if (g_iotHubClient)
        return true;

    const char* connectionString = std::getenv("AZURE_IOT_HUB_CONNECTION_STRING");
    if (connectionString == nullptr || connectionString[0] == '\0')
    {
        std::cerr << "AZURE_IOT_HUB_CONNECTION_STRING is not set" << std::endl;
        return false;
    }

    IoTHub_Init();

    g_iotHubClient = IoTHubDeviceClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
    if (!g_iotHubClient)
    {
        std::cerr << "Failed to create IoT Hub client" << std::endl;
        IoTHub_Deinit();
        return false;
    }

    int messageTimeout = 240000;
    IoTHubDeviceClient_LL_SetOption(g_iotHubClient, "messageTimeout", &messageTimeout);
    g_iotHubInitialized.store(true);
    return true;
}

static void cleanup_iot_hub_client()
{
    if (g_iotHubClient)
    {
        IoTHubDeviceClient_LL_Destroy(g_iotHubClient);
        g_iotHubClient = nullptr;
    }
    if (g_iotHubInitialized.exchange(false))
    {
        IoTHub_Deinit();
    }
}

static bool send_iot_hub_message(const std::string& payload)
{
    if (!init_iot_hub_client())
        return false;

    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromString(payload.c_str());
    if (!messageHandle)
        return false;

    auto result = IoTHubDeviceClient_LL_SendEventAsync(g_iotHubClient, messageHandle, iothub_send_confirmation_callback, nullptr);
    IoTHubMessage_Destroy(messageHandle);

    if (result != IOTHUB_CLIENT_OK)
    {
        std::cerr << "IoT Hub send failed" << std::endl;
        return false;
    }

    IoTHubDeviceClient_LL_DoWork(g_iotHubClient);
    return true;
}

void runSocketReceiver(const std::filesystem::path& outputPath, int port = 5000)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) return;
    if (listen(server_fd, 4) < 0) return;

    std::cout << "Gateway listening on port " << port << std::endl;

    while (g_run.load())
    {
        sockaddr_in client{};
        socklen_t client_len = sizeof(client);
        int client_fd = accept(server_fd, (sockaddr*)&client, &client_len);
        if (client_fd < 0) continue;

        std::string buffer;
        char tmp[4096];
        ssize_t r;
        while ((r = recv(client_fd, tmp, sizeof(tmp), 0)) > 0)
            buffer.append(tmp, tmp + r);
        close(client_fd);

        if (!buffer.empty())
        {
            std::ofstream ofs(outputPath, std::ios::app);
            if (ofs)
            {
                ofs << buffer << "\n";
            }
        }
    }

    close(server_fd);
}

void Gateway::initialize()
{
    std::cout << "Gateway initialized" << std::endl;
}

void Gateway::run()
{
    std::filesystem::path outputFile = std::filesystem::current_path() / "logs" / "jetson_data.jsonl";
    std::filesystem::path jetsonLogsDir = std::filesystem::current_path() / ".." / "jetson-ai" / "logs";

    std::string payload = router::compute_optimal_route_payload(jetsonLogsDir, outputFile, gateway_intersection_id());
    if (!send_iot_hub_message(payload))
    {
        std::cerr << "Failed to send optimal route payload to Azure IoT Hub" << std::endl;
    }

    std::thread receiver(runSocketReceiver, outputFile, 5000);

    std::cout << "Gateway running (socket receiver)" << std::endl;
    while (g_run.load())
        std::this_thread::sleep_for(std::chrono::seconds(1));

    receiver.join();
    cleanup_iot_hub_client();
}
