
#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <mutex>

struct Detection
{
    int class_id;
    float confidence;
    cv::Rect bbox;
};

class TrafficDetector
{
public:
    TrafficDetector();
    bool initialize();
    std::vector<Detection> detect(const cv::Mat& frame);

private:
    cv::Mat lastGray_;
    bool initialized_;
    std::mutex mtx_;
    double minArea_;
};
