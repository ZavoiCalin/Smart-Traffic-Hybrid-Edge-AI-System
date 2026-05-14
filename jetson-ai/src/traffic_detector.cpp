
#include "traffic_detector.hpp"
#include <opencv2/imgproc.hpp>

TrafficDetector::TrafficDetector()
    : initialized_(false), minArea_(500.0)
{
}

bool TrafficDetector::initialize()
{
    std::lock_guard<std::mutex> lk(mtx_);
    initialized_ = true;
    lastGray_ = cv::Mat();
    return true;
}

std::vector<Detection> TrafficDetector::detect(const cv::Mat& frame)
{
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<Detection> detections;

    if (!initialized_ || frame.empty())
        return detections;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5,5), 0);

    if (lastGray_.empty())
    {
        lastGray_ = gray.clone();
        return detections;
    }

    cv::Mat diff;
    cv::absdiff(lastGray_, gray, diff);
    cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);
    cv::morphologyEx(diff, diff, cv::MORPH_CLOSE, cv::Mat(), cv::Point(-1,-1), 2);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(diff, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& c : contours)
    {
        double area = cv::contourArea(c);
        if (area < minArea_) continue;
        cv::Rect bbox = cv::boundingRect(c);
        Detection d;
        d.class_id = 0;
        d.confidence = 0.6f;
        d.bbox = bbox;
        detections.push_back(d);
    }

    lastGray_ = gray.clone();
    return detections;
}
