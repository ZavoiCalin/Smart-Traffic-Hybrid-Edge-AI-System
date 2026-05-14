
#include "temporal_filter.h"

static float buffer[16];
static int buf_idx = 0;
static int buf_count = 0;
static const float alpha = 0.2f;
static float last_output = 0.0f;

void temporal_smoothing()
{
	float sample = 0.0f;
	sample = (float)(buf_count + 1);

	buffer[buf_idx] = sample;
	buf_idx = (buf_idx + 1) % 16;
	if (buf_count < 16) buf_count++;

	if (buf_count == 1)
		last_output = sample;
	else
		last_output = alpha * sample + (1.0f - alpha) * last_output;

	(void)last_output;
}
