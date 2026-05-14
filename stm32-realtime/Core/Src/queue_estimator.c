
#include "queue_estimator.h"

static float last_density = 0.0f;
static float last_flow = 0.0f;

void compute_queue_density()
{
	last_density = 0.25f;
	(void)last_density;
}

void compute_flow_rate()
{
	last_flow = 12.5f;
	(void)last_flow;
}
