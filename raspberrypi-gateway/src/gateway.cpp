
#include "gateway.hpp"
#include <iostream>
#include <thread>
#include <chrono>

void Gateway::initialize()
{
    std::cout << "Gateway initialized" << std::endl;
}

void Gateway::run()
{
    std::cout << "Gateway running (prototype loop)" << std::endl;
    for (int i = 0; i < 5; ++i)
    {
        std::cout << "Gateway: sending heartbeat " << i+1 << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Gateway prototype run complete" << std::endl;
}
