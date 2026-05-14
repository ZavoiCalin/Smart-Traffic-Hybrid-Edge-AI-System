
#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <mutex>

struct Track
{
    int id;
    cv::Rect bbox;
};

class Tracker
{
public:
    Tracker();
    void update(const std::vector<cv::Rect>& detections);
    std::vector<Track> getTracks();

private:
    float iou(const cv::Rect& a, const cv::Rect& b);
    std::vector<Track> tracks_;
    std::vector<int> ages_;
    int next_id_;
    float iouThreshold_;
    std::mutex mtx_;
};
