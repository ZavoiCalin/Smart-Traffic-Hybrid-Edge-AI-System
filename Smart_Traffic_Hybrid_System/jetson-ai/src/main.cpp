
#include <opencv2/opencv.hpp>
#include <iostream>
#include "traffic_detector.hpp"

int main()
{
    cv::VideoCapture cap(0);

    if (!cap.isOpened())
    {
        std::cerr << "Failed to open camera" << std::endl;
        return -1;
    }

    TrafficDetector detector;

    detector.initialize();

    cv::Mat frame;

    while (true)
    {
        cap >> frame;

        if (frame.empty())
            continue;

        auto detections = detector.detect(frame);

        for (const auto& d : detections)
        {
            cv::rectangle(frame, d.bbox, cv::Scalar(0,255,0), 2);
        }

        cv::imshow("Traffic", frame);

        if (cv::waitKey(1) == 27)
            break;
    }

    return 0;
}
