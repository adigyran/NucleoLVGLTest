#include "fpu_demo.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include <cstdint>
 
#define FPU_STACK_SIZE 2048
#define FPU_THREAD_PRIO 6
#define FPU_WORK_ITERATIONS 800
 
LOG_MODULE_REGISTER(fpu_demo, LOG_LEVEL_INF);
 
K_THREAD_STACK_DEFINE(fpu_stack_a, FPU_STACK_SIZE);
K_THREAD_STACK_DEFINE(fpu_stack_b, FPU_STACK_SIZE);
static struct k_thread fpu_thread_a;
static struct k_thread fpu_thread_b;
static volatile float fpu_sink_a;
static volatile float fpu_sink_b;
static bool fpu_started;
 
static void fpu_worker(void *p1, void *p2, void *p3)
{
	constexpr float two_pi = 6.2831853F;
	float angle = static_cast<float>(reinterpret_cast<intptr_t>(p1)) * 0.37F;
	volatile float *const sink = reinterpret_cast<volatile float *>(p2);
	ARG_UNUSED(p3);
 
	while (true) {
		float acc = 0.0F;
 
		for (int i = 0; i < FPU_WORK_ITERATIONS; ++i) {
			angle += 0.011F;
			if (angle > two_pi) {
				angle -= two_pi;
			}
			acc += sinf(angle) * cosf(angle * 0.73F);
			acc += sqrtf(1.0F + (angle * 0.25F));
		}
 
		*sink = acc;
		k_msleep(10);
	}
}
 
void start_fpu_demo()
{
	if (fpu_started) {
		return;
	}
 
	k_thread_create(&fpu_thread_a, fpu_stack_a, K_THREAD_STACK_SIZEOF(fpu_stack_a), fpu_worker,
			reinterpret_cast<void *>(1), const_cast<float *>(&fpu_sink_a), nullptr,
			FPU_THREAD_PRIO, K_FP_REGS, K_NO_WAIT);
	k_thread_name_set(&fpu_thread_a, "fpu_a");
 
	k_thread_create(&fpu_thread_b, fpu_stack_b, K_THREAD_STACK_SIZEOF(fpu_stack_b), fpu_worker,
			reinterpret_cast<void *>(2), const_cast<float *>(&fpu_sink_b), nullptr,
			FPU_THREAD_PRIO, K_FP_REGS, K_NO_WAIT);
	k_thread_name_set(&fpu_thread_b, "fpu_b");
 
	fpu_started = true;
	LOG_INF("FPU threads started");
}