#include "cpp_examples.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
 
#define CPP_EXAMPLE_LOG_EVERY_N_TICKS 12
 
LOG_MODULE_REGISTER(cpp_examples, LOG_LEVEL_INF);
 
namespace {
float avg4(const float *items)
{
	float sum = 0.0F;
	for (int i = 0; i < 4; ++i) {
		sum += items[i];
	}
	return sum / 4.0F;
}
 
class ScopedElapsedMs {
public:
	ScopedElapsedMs(int64_t *out_elapsed) : started_ms_(k_uptime_get()), out_elapsed_(out_elapsed) {}
	~ScopedElapsedMs()
	{
		if (out_elapsed_ != nullptr) {
			*out_elapsed_ = k_uptime_get() - started_ms_;
		}
	}
 
private:
	int64_t started_ms_;
	int64_t *out_elapsed_;
};
} // namespace
 
void cpp_examples_tick(uint32_t tick)
{
	int64_t elapsed = 0;
	const float samples[4] = {1.0F, 2.0F, 3.0F, static_cast<float>(tick % 10U)};
	const float mean = avg4(samples);
 
	{
		ScopedElapsedMs elapsed_scope(&elapsed);
		k_busy_wait(200);
	}
 
	if ((tick % CPP_EXAMPLE_LOG_EVERY_N_TICKS) == 0U) {
		LOG_INF("C++ sample avg=%.2f elapsed=%lldms", static_cast<double>(mean),
			static_cast<long long>(elapsed));
	}
}