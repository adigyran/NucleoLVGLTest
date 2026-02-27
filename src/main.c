#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_cpp.h"
 
#define LED_NODE DT_ALIAS(led0)
 
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
 
LOG_MODULE_REGISTER(app_entry, LOG_LEVEL_INF);
 
int main(void)
{
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device is not ready");
		return 0;
	}
 
	if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) {
		LOG_ERR("Failed to configure LED pin");
		return 0;
	}
 
	LOG_INF("C entry ready, handing over to C++ app");
	return app_cpp_run();
}