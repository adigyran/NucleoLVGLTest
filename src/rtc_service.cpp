#include "rtc_service.hpp"
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
 
#define RTC_NODE DT_NODELABEL(rtc)
 
static const struct device *const rtc_dev = DEVICE_DT_GET(RTC_NODE);
 
LOG_MODULE_REGISTER(rtc_service, LOG_LEVEL_INF);
 
bool rtc_service_get(struct rtc_time *out_time)
{
	if ((out_time == nullptr) || !device_is_ready(rtc_dev)) {
		return false;
	}
 
	return rtc_get_time(rtc_dev, out_time) == 0;
}
 
bool rtc_service_set(const struct rtc_time *in_time)
{
	if ((in_time == nullptr) || !device_is_ready(rtc_dev)) {
		return false;
	}
 
	return rtc_set_time(rtc_dev, in_time) == 0;
}
 
bool rtc_service_init()
{
	struct rtc_time now;
 
	if (!device_is_ready(rtc_dev)) {
		LOG_ERR("RTC device is not ready");
		return false;
	}
 
	if (rtc_service_get(&now)) {
		return true;
	}
 
	struct rtc_time init = {};
	init.tm_sec = 0;
	init.tm_min = 0;
	init.tm_hour = 0;
	init.tm_mday = 1;
	init.tm_mon = 0;
	init.tm_year = 2026 - 1900;
	init.tm_wday = 4;
	init.tm_yday = -1;
	init.tm_isdst = -1;
	init.tm_nsec = 0;
 
	if (!rtc_service_set(&init)) {
		LOG_ERR("Failed to initialize RTC time");
		return false;
	}
 
	return true;
}