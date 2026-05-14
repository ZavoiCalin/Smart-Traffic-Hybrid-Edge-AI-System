
#include "mqtt_client.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>

void publishTrafficEvent()
{
    std::ofstream ofs("traffic_event.log", std::ios::app);
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    if (ofs.is_open())
    {
        ofs << std::ctime(&t) << " - traffic event (prototype)\n";
        ofs.close();
    }

    std::cout << "[MQTT] traffic event published (prototype)" << std::endl;
}
