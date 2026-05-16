#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>

#include "traffic_detector.hpp"
#include "tracker.hpp"

using namespace std::chrono_literals;

static constexpr char kDefaultIntersectionId[] = "iA";

static std::string iso_utc_now(const std::chrono::system_clock::time_point& tp)
{
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    // Use thread-safe gmtime_r on Linux/Jetson; fall back to std::gmtime if necessary
    if (gmtime_r(&t, &tm) == nullptr)
    {
        auto g = std::gmtime(&t);
        if (g) tm = *g;
    }
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

static std::string iso_utc_now()
{
    return iso_utc_now(std::chrono::system_clock::now());
}

static std::string filename_timestamp(const std::chrono::system_clock::time_point& tp)
{
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    if (gmtime_r(&t, &tm) == nullptr)
    {
        auto g = std::gmtime(&t);
        if (g) tm = *g;
    }
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%dT%H%M%SZ");
    return ss.str();
}

static std::string filename_timestamp()
{
    return filename_timestamp(std::chrono::system_clock::now());
}

static bool atomic_write_json(const std::filesystem::path& path, const std::string& content)
{
    try
    {
        auto tmp = path;
        tmp += ".tmp";
        std::ofstream ofs(tmp, std::ios::trunc);
        if (!ofs) return false;
        ofs << content;
        ofs.close();
        std::filesystem::rename(tmp, path);
        return true;
    }
    catch (...) { return false; }
}

int main(int argc, char** argv)
{
    std::string intersectionId = kDefaultIntersectionId;
    if (argc > 1 && argv[1] != nullptr)
    {
        intersectionId = argv[1];
    }

    std::filesystem::path outDir = std::filesystem::current_path() / "jetson-ai";
    std::filesystem::path logsDir = outDir / "logs";
    std::error_code ec;
    std::filesystem::create_directories(logsDir, ec);
    if (ec)
    {
        std::cerr << "Failed to create logs directory: " << ec.message() << std::endl;
    }

    const std::chrono::seconds logInterval(3);
    const int maxLogEntries = 30;
    std::vector<std::string> logEntries;
    auto lastLog = std::chrono::steady_clock::now();

    cv::VideoCapture cap(0);

    if (!cap.isOpened())
    {
        std::cerr << "Failed to open camera" << std::endl;
        return -1;
    }

    TrafficDetector detector;
    Tracker tracker;

    detector.initialize();

    const std::chrono::seconds logInterval(3);
    const int maxLogEntries = 30;
    std::vector<std::string> logEntries;
    auto lastLog = std::chrono::steady_clock::now();

    cv::Mat frame;

    while (true)
    {
        cap >> frame;

        if (frame.empty())
            continue;

        auto detections = detector.detect(frame);

        // Convert detections to rects for tracker
        std::vector<cv::Rect> detRects;
        for (const auto& d : detections)
            detRects.push_back(d.bbox);

        tracker.update(detRects);
        auto tracks = tracker.getTracks();

        // draw tracks
        for (const auto& t : tracks)
        {
            cv::rectangle(frame, t.bbox, cv::Scalar(0,255,0), 2);
            cv::putText(frame, std::to_string(t.id), cv::Point(t.bbox.x, t.bbox.y-5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0,255,0), 1);
        }

        cv::imshow("Traffic", frame);

        // logging every N seconds
        auto now = std::chrono::steady_clock::now();
        if (now - lastLog >= logInterval)
        {
            std::ostringstream entry;
            entry << "{";
            entry << "\"timestamp\":\"" << iso_utc_now() << "\",";
            entry << "\"vehicle_count\":" << tracks.size();
            entry << "}";
            logEntries.push_back(entry.str());

            lastLog = now;
            if ((int)logEntries.size() >= maxLogEntries)
            {
                std::ostringstream ss;
                ss << "{";
                ss << "\"" << intersectionId << "\":";
                ss << "[";
                for (size_t i = 0; i < logEntries.size(); ++i)
                {
                    if (i > 0) ss << ",";
                    ss << logEntries[i];
                }
                ss << "]";
                ss << "}";

                auto fname = std::string("log_") + intersectionId + "_" + filename_timestamp() + ".json";
                auto full = logsDir / fname;
                if (!atomic_write_json(full, ss.str()))
                {
                    std::cerr << "Failed to write log: " << full.string() << std::endl;
                }

                std::cout << "Generated log for intersection " << intersectionId << " with " << logEntries.size() << " entries. Exiting." << std::endl;
                break;
            }
        }

        if (cv::waitKey(1) == 27)
            break;
    }

    return 0;
}
