#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <cstdint>
#include "app_cpp.h"
#include "cpp_examples.hpp"
#include "fpu_demo.hpp"
#include "lvgl_demo.hpp"
#include "msgq_demo.hpp"
#include "rtc_service.hpp"
 
#define STATUS_PERIOD_MS 500
#define LVGL_PERIOD_MS 16
#define LED0_BLINK_MS 100
#define LED1_BLINK_MS 250
#define LED2_BLINK_MS 500
#define LED_STACK_SIZE 512
#define LED_THREAD_PRIO 9
 
static const gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#if DT_NODE_EXISTS(DT_ALIAS(led1))
static const gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led2))
static const gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
#endif
 
struct led_ctx {
	const gpio_dt_spec *led;
	uint32_t period_ms;
};
 
K_THREAD_STACK_DEFINE(led0_stack, LED_STACK_SIZE);
static struct k_thread led0_thread;
static struct led_ctx led0_ctx = { .led = &led0, .period_ms = LED0_BLINK_MS };
#if DT_NODE_EXISTS(DT_ALIAS(led1))
K_THREAD_STACK_DEFINE(led1_stack, LED_STACK_SIZE);
static struct k_thread led1_thread;
static struct led_ctx led1_ctx = { .led = &led1, .period_ms = LED1_BLINK_MS };
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led2))
K_THREAD_STACK_DEFINE(led2_stack, LED_STACK_SIZE);
static struct k_thread led2_thread;
static struct led_ctx led2_ctx = { .led = &led2, .period_ms = LED2_BLINK_MS };
#endif
 
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);
 
static void led_worker(void *p1, void *p2, void *p3)
{
	auto *ctx = static_cast<const struct led_ctx *>(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
 
	while (true) {
		(void)gpio_pin_toggle_dt(ctx->led);
		k_msleep(static_cast<int32_t>(ctx->period_ms));
	}
}
 
static void start_led_blinker(struct led_ctx *ctx, struct k_thread *thread, k_thread_stack_t *stack,
			      size_t stack_size, const char *name)
{
	if (!gpio_is_ready_dt(ctx->led)) {
		LOG_WRN("%s is not ready", name);
		return;
	}
 
	if (gpio_pin_configure_dt(ctx->led, GPIO_OUTPUT_INACTIVE) < 0) {
		LOG_WRN("Failed to configure %s", name);
		return;
	}
 
	k_thread_create(thread, stack, stack_size, led_worker, ctx, nullptr, nullptr, LED_THREAD_PRIO, 0,
			K_NO_WAIT);
	k_thread_name_set(thread, name);
}
 
int app_cpp_run(void)
{
	uint32_t ticks = 0;
	uint32_t status_ticks = 0;
 
	if (!rtc_service_init()) {
		return 0;
	}
 
	// start_fpu_demo();
	// start_msgq_demo();
	lvgl_demo_init();
	start_led_blinker(&led0_ctx, &led0_thread, led0_stack, K_THREAD_STACK_SIZEOF(led0_stack),
			  "led0_100ms");
#if DT_NODE_EXISTS(DT_ALIAS(led1))
	start_led_blinker(&led1_ctx, &led1_thread, led1_stack, K_THREAD_STACK_SIZEOF(led1_stack),
			  "led1_250ms");
#else
	LOG_WRN("led1 alias is missing in devicetree");
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led2))
	start_led_blinker(&led2_ctx, &led2_thread, led2_stack, K_THREAD_STACK_SIZEOF(led2_stack),
			  "led2_500ms");
#else
	LOG_WRN("led2 alias is missing in devicetree");
#endif
	LOG_INF("C++ demos started");
 
	while (true) {
		ticks += LVGL_PERIOD_MS;
		status_ticks += LVGL_PERIOD_MS;
		lvgl_demo_tick(ticks);
		if (status_ticks >= STATUS_PERIOD_MS) {
			status_ticks = 0;
			cpp_examples_tick(ticks / STATUS_PERIOD_MS);
		}
 
		k_msleep(LVGL_PERIOD_MS);
	}
}
