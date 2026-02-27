#include "rtc_service.hpp"
#include <zephyr/shell/shell.h>
#include <errno.h>
#include <stdlib.h>
 
static int cmd_rtc_get(const struct shell *sh, size_t argc, char **argv)
{
	struct rtc_time now;
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
 
	if (!rtc_service_get(&now)) {
		shell_error(sh, "Failed to read RTC time");
		return -EIO;
	}
 
	shell_print(sh, "%04d-%02d-%02d %02d:%02d:%02d",
		    now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min,
		    now.tm_sec);
	return 0;
}
 
static bool parse_part(const char *text, int *out_value)
{
	char *end = nullptr;
	long value;
 
	if ((text == nullptr) || (out_value == nullptr)) {
		return false;
	}
 
	value = strtol(text, &end, 10);
	if ((end == text) || (*end != '\0')) {
		return false;
	}
 
	*out_value = static_cast<int>(value);
	return true;
}
 
static int cmd_rtc_set(const struct shell *sh, size_t argc, char **argv)
{
	struct rtc_time t = {};
 
	if (argc != 7) {
		shell_error(sh, "Usage: rtc set <YYYY> <MM> <DD> <hh> <mm> <ss>");
		return -EINVAL;
	}
 
	if (!parse_part(argv[1], &t.tm_year) || !parse_part(argv[2], &t.tm_mon) ||
	    !parse_part(argv[3], &t.tm_mday) || !parse_part(argv[4], &t.tm_hour) ||
	    !parse_part(argv[5], &t.tm_min) || !parse_part(argv[6], &t.tm_sec)) {
		shell_error(sh, "Invalid number format");
		return -EINVAL;
	}
 
	t.tm_year -= 1900;
	t.tm_mon -= 1;
	t.tm_isdst = -1;
 
	if (!rtc_service_set(&t)) {
		shell_error(sh, "Failed to set RTC time");
		return -EIO;
	}
 
	shell_print(sh, "RTC time updated");
	return 0;
}
 
SHELL_STATIC_SUBCMD_SET_CREATE(sub_rtc, SHELL_CMD(get, NULL, "Read RTC time", cmd_rtc_get),
			       SHELL_CMD(set, NULL,
					 "Set RTC time: set <YYYY> <MM> <DD> <hh> <mm> <ss>",
					 cmd_rtc_set),
			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(rtc, &sub_rtc, "RTC commands", NULL);