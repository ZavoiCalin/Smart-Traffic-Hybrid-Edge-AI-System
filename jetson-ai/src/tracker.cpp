
#include "tracker.hpp"
#include <algorithm>

Tracker::Tracker()
	: next_id_(1), iouThreshold_(0.3f)
{
}

float Tracker::iou(const cv::Rect& a, const cv::Rect& b)
{
	int x1 = std::max(a.x, b.x);
	int y1 = std::max(a.y, b.y);
	int x2 = std::min(a.x + a.width, b.x + b.width);
	int y2 = std::min(a.y + a.height, b.y + b.height);
	int w = std::max(0, x2 - x1);
	int h = std::max(0, y2 - y1);
	float inter = (float)(w * h);
	float uni = (float)(a.area() + b.area() - inter);
	return uni > 0.0f ? inter / uni : 0.0f;
}

void Tracker::update(const std::vector<cv::Rect>& detections)
{
	std::lock_guard<std::mutex> lk(mtx_);

	std::vector<int> matched(tracks_.size(), -1);
	std::vector<bool> detUsed(detections.size(), false);

	for (size_t t = 0; t < tracks_.size(); ++t)
	{
		float bestIou = iouThreshold_;
		int bestIdx = -1;
		for (size_t d = 0; d < detections.size(); ++d)
		{
			if (detUsed[d]) continue;
			float cur = iou(tracks_[t].bbox, detections[d]);
			if (cur > bestIou)
			{
				bestIou = cur;
				bestIdx = (int)d;
			}
		}
		if (bestIdx >= 0)
		{
			tracks_[t].bbox = detections[bestIdx];
			ages_[t] = 0;
			detUsed[bestIdx] = true;
			matched[t] = bestIdx;
		}
		else
		{
			ages_[t]++;
		}
	}

	for (size_t d = 0; d < detections.size(); ++d)
	{
		if (detUsed[d]) continue;
		Track tr;
		tr.id = next_id_++;
		tr.bbox = detections[d];
		tracks_.push_back(tr);
		ages_.push_back(0);
	}

	const int maxAge = 5;
	for (int i = (int)tracks_.size() - 1; i >= 0; --i)
	{
		if (ages_[i] > maxAge)
		{
			tracks_.erase(tracks_.begin() + i);
			ages_.erase(ages_.begin() + i);
		}
	}
}

std::vector<Track> Tracker::getTracks()
{
	std::lock_guard<std::mutex> lk(mtx_);
	return tracks_;
}
