
cmake_minimum_required(VERSION 3.10)

project(SmartTrafficSystem)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)
find_package(CUDA REQUIRED)

include_directories(
    include
    ${OpenCV_INCLUDE_DIRS}
)

add_executable(traffic_system
    src/main.cpp
    src/traffic_detector.cpp
    src/tracker.cpp
    src/mqtt_client.cpp
)

target_link_libraries(
    traffic_system
    ${OpenCV_LIBS}
)
