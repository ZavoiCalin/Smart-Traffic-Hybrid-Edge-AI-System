
#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

struct Detection
{
    int class_id;
    float confidence;
    cv::Rect bbox;
};

class TrafficDetector
{
public:
    bool initialize();
    std::vector<Detection> detect(const cv::Mat& frame);
};
