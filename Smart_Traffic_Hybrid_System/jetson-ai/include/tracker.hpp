
#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

struct Track
{
    int id;
    cv::Rect bbox;
};

class Tracker
{
public:
    void update(const std::vector<cv::Rect>& detections);
};
