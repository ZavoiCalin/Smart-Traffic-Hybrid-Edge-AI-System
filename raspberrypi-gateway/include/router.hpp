#pragma once

#include <filesystem>
#include <string>

namespace router
{
    std::string compute_optimal_route_payload(
        const std::filesystem::path& jetsonLogsDir,
        const std::filesystem::path& mergedLogPath,
        const std::string& gatewayIntersectionId);
}
