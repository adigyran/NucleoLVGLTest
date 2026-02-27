#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_cpp.h"
 
LOG_MODULE_REGISTER(app_entry, LOG_LEVEL_INF);
 
int main(void)
{
	LOG_INF("C entry ready");
	return app_cpp_run();
}