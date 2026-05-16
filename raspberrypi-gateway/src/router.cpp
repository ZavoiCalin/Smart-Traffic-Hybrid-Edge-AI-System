#include "router.hpp"
#include "route_optimizer.hpp"
#include <chrono>
#include <ctime>

namespace router
{
static std::string current_utc_timestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};

#if defined(_WIN32)
    gmtime_s(&tm, &now_time);
#else
    gmtime_r(&now_time, &tm);
#endif

    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm) == 0)
        return {};
    return std::string(buffer);
}

std::string compute_optimal_route_payload(
    const std::filesystem::path& jetsonLogsDir,
    const std::filesystem::path& mergedLogPath,
    const std::string& gatewayIntersectionId)
{
    auto graph = build_road_graph();
    int totalVehicleCount = merge_jetson_logs(jetsonLogsDir, mergedLogPath);
    auto weights = build_route_weights(graph, mergedLogPath);

    std::string start = gatewayIntersectionId;
    if (graph.find(start) == graph.end())
    {
        start = graph.empty() ? std::string() : graph.begin()->first;
    }

    std::string finish = "iD";
    if (graph.find(finish) == graph.end() && !graph.empty())
    {
        finish = graph.begin()->first;
    }

    auto [route, routeCost] = find_optimal_route(graph, weights, start, finish);
    if (route.empty() && !start.empty())
    {
        route = {start};
    }

    return build_optimal_route_payload(gatewayIntersectionId, route, routeCost, totalVehicleCount);
}

} // namespace router

std::string iso_utc_now()
{
    return router::current_utc_timestamp();
}
