#include "top_stats.hpp"
#include "monitor/top_collector.hpp"
#include "monitor/top_renderer.hpp"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
 
#define TOP_STACK_SIZE 2048
#define TOP_PRIO 8
 
K_THREAD_STACK_DEFINE(top_stack, TOP_STACK_SIZE);
static struct k_thread top_thread;
static bool top_started;
 
static void top_worker(void *p1, void *p2, void *p3)
{
	monitor::TopSnapshot snap = {};
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
 
	monitor::draw_layout_once();
	while (true) {
		monitor::collect_top_snapshot(&snap);
		monitor::render_top_snapshot(&snap);
		k_sleep(K_SECONDS(1));
	}
}
 
bool top_stats_is_running()
{
	return top_started;
}
 
int top_stats_start()
{
	if (top_started) {
		return -EALREADY;
	}
 
	k_thread_create(&top_thread, top_stack, K_THREAD_STACK_SIZEOF(top_stack), top_worker, nullptr,
			nullptr, nullptr, TOP_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&top_thread, "top_stats");
	top_started = true;
	return 0;
}
 
int top_stats_stop()
{
	if (!top_started) {
		return -EALREADY;
	}
 
	k_thread_abort(&top_thread);
	top_started = false;
	printk("\x1b[0m\x1b[?25h\n");
	return 0;
}
 
static int cmd_top_start(const struct shell *sh, size_t argc, char **argv)
{
	int rc;
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
 
	rc = top_stats_start();
	if (rc == 0) {
		shell_print(sh, "top started");
		return 0;
	}
 
	shell_warn(sh, "top already running");
	return rc;
}
 
static int cmd_top_stop(const struct shell *sh, size_t argc, char **argv)
{
	int rc;
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
 
	rc = top_stats_stop();
	if (rc == 0) {
		shell_print(sh, "top stopped");
		return 0;
	}
 
	shell_warn(sh, "top is not running");
	return rc;
}
 
static int cmd_top_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_print(sh, "top: %s", top_stats_is_running() ? "running" : "stopped");
	return 0;
}
 
SHELL_STATIC_SUBCMD_SET_CREATE(sub_top,
	SHELL_CMD(start, NULL, "Start top monitor", cmd_top_start),
	SHELL_CMD(stop, NULL, "Stop top monitor", cmd_top_stop),
	SHELL_CMD(status, NULL, "Show top monitor status", cmd_top_status),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(top, &sub_top, "Top monitor commands", NULL);