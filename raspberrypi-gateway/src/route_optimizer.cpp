#include "route_optimizer.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <limits>
#include <set>
#include <functional>
#include <memory>
#include <cctype>

namespace
{
static std::string extract_json_string_field(const std::string& json, const std::string& field)
{
    size_t pos = json.find(field);
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + field.size());
    if (pos == std::string::npos) return {};
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return json.substr(pos + 1, end - pos - 1);
}

static int extract_json_int_field(const std::string& json, const std::string& field)
{
    size_t pos = json.find(field);
    if (pos == std::string::npos) return 0;
    pos = json.find(':', pos + field.size());
    if (pos == std::string::npos) return 0;
    size_t start = json.find_first_not_of(" \t\r\n", pos + 1);
    if (start == std::string::npos) return 0;
    size_t end = start;
    if (json[end] == '-') ++end;
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) ++end;
    if (end == start) return 0;
    return std::stoi(json.substr(start, end - start));
}

static void extract_json_objects(const std::string& s, const std::function<void(const std::string&)>& cb)
{
    size_t i = 0;
    while (i < s.size())
    {
        size_t start = s.find('{', i);
        if (start == std::string::npos) break;
        int depth = 0;
        size_t j = start;
        for (; j < s.size(); ++j)
        {
            if (s[j] == '{') ++depth;
            else if (s[j] == '}')
            {
                --depth;
                if (depth == 0) break;
            }
        }
        if (depth == 0 && j < s.size())
        {
            cb(s.substr(start, j - start + 1));
            i = j + 1;
        }
        else
        {
            break;
        }
    }
}

static std::string extract_intersection_id(const std::string& json)
{
    return extract_json_string_field(json, "\"intersection_id\"");
}

struct TreeNode
{
    bool leaf = false;
    int featureIndex = 0;
    double threshold = 0.0;
    double value = 0.0;
    std::unique_ptr<TreeNode> left;
    std::unique_ptr<TreeNode> right;
};

static double mean_value(const std::vector<double>& values, const std::vector<int>& indices)
{
    if (indices.empty()) return 0.0;
    double sum = 0.0;
    for (int idx : indices) sum += values[idx];
    return sum / indices.size();
}

static double mse(const std::vector<double>& values, const std::vector<int>& indices)
{
    if (indices.empty()) return 0.0;
    double mean = mean_value(values, indices);
    double sum = 0.0;
    for (int idx : indices)
    {
        double diff = values[idx] - mean;
        sum += diff * diff;
    }
    return sum / indices.size();
}

static std::unique_ptr<TreeNode> build_regression_tree(
    const std::vector<std::vector<double>>& X,
    const std::vector<double>& y,
    const std::vector<int>& sampleIndices,
    const std::vector<int>& featureIndices,
    int depth,
    int maxDepth,
    int minSamplesSplit,
    std::mt19937& rng)
{
    auto node = std::make_unique<TreeNode>();
    node->value = mean_value(y, sampleIndices);
    if (depth >= maxDepth || sampleIndices.size() <= (size_t)minSamplesSplit)
    {
        node->leaf = true;
        return node;
    }

    double bestMSE = mse(y, sampleIndices);
    int bestFeature = -1;
    double bestThreshold = 0.0;
    std::vector<int> bestLeft;
    std::vector<int> bestRight;

    for (int feature : featureIndices)
    {
        std::vector<double> values;
        values.reserve(sampleIndices.size());
        for (int idx : sampleIndices)
            values.push_back(X[idx][feature]);
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());

        for (double threshold : values)
        {
            std::vector<int> left, right;
            for (int idx : sampleIndices)
            {
                if (X[idx][feature] <= threshold)
                    left.push_back(idx);
                else
                    right.push_back(idx);
            }
            if (left.empty() || right.empty()) continue;
            double mseLeft = mse(y, left);
            double mseRight = mse(y, right);
            double weightedMSE = (mseLeft * left.size() + mseRight * right.size()) / sampleIndices.size();
            if (weightedMSE < bestMSE)
            {
                bestMSE = weightedMSE;
                bestFeature = feature;
                bestThreshold = threshold;
                bestLeft = std::move(left);
                bestRight = std::move(right);
            }
        }
    }

    if (bestFeature < 0)
    {
        node->leaf = true;
        return node;
    }

    node->featureIndex = bestFeature;
    node->threshold = bestThreshold;
    node->left = build_regression_tree(X, y, bestLeft, featureIndices, depth + 1, maxDepth, minSamplesSplit, rng);
    node->right = build_regression_tree(X, y, bestRight, featureIndices, depth + 1, maxDepth, minSamplesSplit, rng);
    return node;
}

static double predict_tree(const TreeNode* node, const std::vector<double>& features)
{
    if (!node || node->leaf) return node ? node->value : 0.0;
    if (features[node->featureIndex] <= node->threshold)
        return predict_tree(node->left.get(), features);
    return predict_tree(node->right.get(), features);
}

class RandomForestRegressor
{
public:
    RandomForestRegressor(int nTrees = 5, int maxDepth = 4, int minSamplesSplit = 2)
        : nTrees_(nTrees), maxDepth_(maxDepth), minSamplesSplit_(minSamplesSplit), rng_(std::random_device{}())
    {
    }

    void fit(const std::vector<std::vector<double>>& X, const std::vector<double>& y)
    {
        if (X.empty() || y.empty() || X.size() != y.size()) return;
        int featureCount = static_cast<int>(X[0].size());
        std::vector<int> allFeatures(featureCount);
        std::iota(allFeatures.begin(), allFeatures.end(), 0);

        for (int i = 0; i < nTrees_; ++i)
        {
            std::vector<int> sampleIndices;
            sampleIndices.reserve(X.size());
            std::uniform_int_distribution<int> dist(0, static_cast<int>(X.size()) - 1);
            for (size_t j = 0; j < X.size(); ++j)
                sampleIndices.push_back(dist(rng_));

            std::shuffle(allFeatures.begin(), allFeatures.end(), rng_);
            int featureSubset = std::max(1, featureCount / 2);
            std::vector<int> featureIndices(allFeatures.begin(), allFeatures.begin() + featureSubset);
            trees_.push_back(build_regression_tree(X, y, sampleIndices, featureIndices, 0, maxDepth_, minSamplesSplit_, rng_));
        }
    }

    double predict(const std::vector<double>& features) const
    {
        if (trees_.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& tree : trees_)
            sum += predict_tree(tree.get(), features);
        return sum / trees_.size();
    }

private:
    int nTrees_;
    int maxDepth_;
    int minSamplesSplit_;
    std::mt19937 rng_;
    std::vector<std::unique_ptr<TreeNode>> trees_;
};

} // namespace

std::map<std::string, std::map<std::string, double>> build_road_graph()
{
    return {
        {"iA", {{"iB", 5.0}, {"iC", 10.0}, {"iD", 8.0}}},
        {"iB", {{"iA", 5.0}, {"iC", 4.0}, {"iD", 7.0}}},
        {"iC", {{"iA", 10.0}, {"iB", 4.0}, {"iD", 6.0}}},
        {"iD", {{"iA", 8.0}, {"iB", 7.0}, {"iC", 6.0}}}
    };
}

int merge_jetson_logs(const std::filesystem::path& jetsonLogsDir,
                      const std::filesystem::path& outputJsonlPath)
{
    std::error_code ec;
    std::filesystem::create_directories(outputJsonlPath.parent_path(), ec);
    std::ofstream ofs(outputJsonlPath, std::ios::trunc);
    if (!ofs) return 0;

    int totalVehicleCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(jetsonLogsDir, ec))
    {
        if (ec || !entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        std::ifstream ifs(entry.path());
        if (!ifs) continue;

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::string intersectionId = extract_intersection_id(content);
        if (intersectionId.empty()) continue;

        size_t arrayStart = content.find('[', content.find(intersectionId));
        if (arrayStart == std::string::npos) continue;

        int depth = 0;
        size_t arrayEnd = arrayStart;
        for (; arrayEnd < content.size(); ++arrayEnd)
        {
            if (content[arrayEnd] == '[') ++depth;
            else if (content[arrayEnd] == ']')
            {
                --depth;
                if (depth == 0) break;
            }
        }
        if (arrayEnd >= content.size()) continue;

        std::string arrayText = content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
        extract_json_objects(arrayText, [&](const std::string& obj) {
            std::string body = obj.substr(1, obj.size() - 2);
            ofs << "{\"intersection_id\":\"" << intersectionId << "\"," << body << "}\n";
            totalVehicleCount += extract_json_int_field(obj, "\"vehicle_count\"");
        });
    }

    return totalVehicleCount;
}

std::map<std::string, std::map<std::string, double>> build_route_weights(
    const std::map<std::string, std::map<std::string, double>>& graph,
    const std::filesystem::path& mergedLogPath)
{
    std::map<std::string, int> vehicleCounts;
    std::ifstream ifs(mergedLogPath);
    if (ifs.is_open())
    {
        std::string line;
        while (std::getline(ifs, line))
        {
            std::string intersectionId = extract_json_string_field(line, "\"intersection_id\"");
            int count = extract_json_int_field(line, "\"vehicle_count\"");
            if (!intersectionId.empty())
                vehicleCounts[intersectionId] = count;
        }
    }

    std::vector<std::vector<double>> X;
    std::vector<double> y;
    for (const auto& kv : graph)
    {
        const std::string& start = kv.first;
        int count = vehicleCounts.count(start) ? vehicleCounts[start] : 0;
        for (const auto& edge : kv.second)
        {
            const std::string& end = edge.first;
            double distance = edge.second;
            X.push_back({static_cast<double>(count), distance});
            y.push_back(distance + count * 0.2);
        }
    }

    RandomForestRegressor model(7, 4, 2);
    if (!X.empty())
        model.fit(X, y);

    std::map<std::string, std::map<std::string, double>> weights;
    for (const auto& kv : graph)
    {
        const std::string& start = kv.first;
        int count = vehicleCounts.count(start) ? vehicleCounts[start] : 0;
        for (const auto& edge : kv.second)
        {
            std::vector<double> features = {static_cast<double>(count), edge.second};
            double predicted = model.predict(features);
            if (predicted <= 0.0) predicted = edge.second;
            weights[start][edge.first] = predicted;
        }
    }

    return weights;
}

std::pair<std::vector<std::string>, double> find_optimal_route(
    const std::map<std::string, std::map<std::string, double>>& graph,
    const std::map<std::string, std::map<std::string, double>>& weights,
    const std::string& start,
    const std::string& end)
{
    std::map<std::string, double> dist;
    std::map<std::string, std::string> prev;
    std::set<std::pair<double, std::string>> queue;

    for (const auto& kv : graph)
        dist[kv.first] = std::numeric_limits<double>::infinity();
    if (dist.find(start) == dist.end() || dist.find(end) == dist.end())
        return {{}, 0.0};

    dist[start] = 0.0;
    queue.insert({0.0, start});

    while (!queue.empty())
    {
        auto [currDist, curr] = *queue.begin();
        queue.erase(queue.begin());
        if (curr == end) break;
        for (const auto& edge : graph.at(curr))
        {
            const std::string& neighbor = edge.first;
            double edgeWeight = weights.at(curr).at(neighbor);
            double nd = currDist + edgeWeight;
            if (nd < dist[neighbor])
            {
                queue.erase({dist[neighbor], neighbor});
                dist[neighbor] = nd;
                prev[neighbor] = curr;
                queue.insert({nd, neighbor});
            }
        }
    }

    std::vector<std::string> route;
    std::string current = end;
    if (dist[end] == std::numeric_limits<double>::infinity())
        return {{}, 0.0};

    while (current != start)
    {
        route.insert(route.begin(), current);
        current = prev[current];
    }
    route.insert(route.begin(), start);
    return {route, dist[end]};
}

std::string build_optimal_route_payload(
    const std::string& intersectionId,
    const std::vector<std::string>& route,
    double routeCost,
    int totalVehicleCount)
{
    std::ostringstream ss;
    ss << "{";
    ss << "\"intersection_id\":\"" << intersectionId << "\",";
    ss << "\"timestamp\":\"" << iso_utc_now() << "\",";
    ss << "\"optimal_route\":[";
    for (size_t i = 0; i < route.size(); ++i)
    {
        if (i) ss << ",";
        ss << "\"" << route[i] << "\"";
    }
    ss << "],";
    ss << "\"route_cost\":" << routeCost << ",";
    ss << "\"vehicle_count\":" << totalVehicleCount;
    ss << "}";
    return ss.str();
}
