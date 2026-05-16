#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <utility>

std::map<std::string, std::map<std::string, double>> build_road_graph();
int merge_jetson_logs(const std::filesystem::path& jetsonLogsDir, const std::filesystem::path& outputJsonlPath);
std::map<std::string, std::map<std::string, double>> build_route_weights(
    const std::map<std::string, std::map<std::string, double>>& graph,
    const std::filesystem::path& mergedLogPath);
std::pair<std::vector<std::string>, double> find_optimal_route(
    const std::map<std::string, std::map<std::string, double>>& graph,
    const std::map<std::string, std::map<std::string, double>>& weights,
    const std::string& start,
    const std::string& end);
std::string build_optimal_route_payload(
    const std::string& intersectionId,
    const std::vector<std::string>& route,
    double routeCost,
    int totalVehicleCount);

std::string iso_utc_now();
