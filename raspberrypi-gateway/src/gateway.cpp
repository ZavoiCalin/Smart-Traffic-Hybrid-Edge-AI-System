#include "gateway.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static std::atomic<bool> g_run{true};

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
                if (depth == 0)
                    break;
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
    size_t first_quote = json.find('"');
    if (first_quote == std::string::npos) return {};
    size_t second_quote = json.find('"', first_quote + 1);
    if (second_quote == std::string::npos) return {};
    return json.substr(first_quote + 1, second_quote - first_quote - 1);
}

static void merge_jetson_logs(const std::filesystem::path& jetsonLogsDir,
                              const std::filesystem::path& outputJsonlPath)
{
    std::error_code ec;
    std::filesystem::create_directories(outputJsonlPath.parent_path(), ec);

    std::ofstream ofs(outputJsonlPath, std::ios::trunc);
    if (!ofs) return;

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
        for (; arrayEnd < content.size(); ++array_end)
        {
            if (content[array_end] == '[') ++depth;
            else if (content[array_end] == ']')
            {
                --depth;
                if (depth == 0)
                    break;
            }
        }
        if (array_end >= content.size()) continue;

        std::string arrayText = content.substr(array_start + 1, array_end - array_start - 1);
        extract_json_objects(arrayText, [&](const std::string& obj) {
            std::string body = obj.substr(1, obj.size() - 2);
            ofs << "{\"intersection_id\":\"" << intersectionId << "\"," << body << "}\n";
        });
    }
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

    merge_jetson_logs(jetsonLogsDir, outputFile);

    std::thread receiver(runSocketReceiver, outputFile, 5000);

    std::cout << "Gateway running (socket receiver)" << std::endl;
    while (g_run.load())
        std::this_thread::sleep_for(std::chrono::seconds(1));

    receiver.join();
}
