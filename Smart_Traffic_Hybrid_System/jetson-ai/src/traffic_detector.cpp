
#include "traffic_detector.hpp"

bool TrafficDetector::initialize()
{
    return true;
}

std::vector<Detection> TrafficDetector::detect(const cv::Mat& frame)
{
    std::vector<Detection> detections;

    Detection d;
    d.class_id = 2;
    d.confidence = 0.95f;
    d.bbox = cv::Rect(100,100,200,100);

    detections.push_back(d);

    return detections;
}
