#pragma once
 
#include <zephyr/drivers/rtc.h>
 
bool rtc_service_init();
bool rtc_service_get(struct rtc_time *out_time);
bool rtc_service_set(const struct rtc_time *in_time);