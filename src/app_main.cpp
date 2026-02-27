#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <cstdint>
#include "app_cpp.h"
#include "cpp_examples.hpp"
#include "fpu_demo.hpp"
#include "msgq_demo.hpp"
#include "rtc_service.hpp"
 
#define LED_NODE DT_ALIAS(led0)
#define STATUS_PERIOD_MS 500
#define TIME_LOG_EVERY_N_TICKS 10
 
static const gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
 
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);
 
int app_cpp_run(void)
{
	uint32_t ticks = 0;
 
	if (!rtc_service_init()) {
		return 0;
	}
 
	start_fpu_demo();
	start_msgq_demo();
	LOG_INF("C++ demos started");
 
	while (true) {
		++ticks;
		gpio_pin_toggle_dt(&led);
		cpp_examples_tick(ticks);
 
		if ((ticks % TIME_LOG_EVERY_N_TICKS) == 0U) {
			struct rtc_time now;
			if (rtc_service_get(&now)) {
				LOG_INF("Time: %04d-%02d-%02d %02d:%02d:%02d",
					now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
					now.tm_hour, now.tm_min, now.tm_sec);
			}
		}
 
		k_msleep(STATUS_PERIOD_MS);
	}
}