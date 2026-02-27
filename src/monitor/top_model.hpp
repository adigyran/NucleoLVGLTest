#pragma once
 
#include <zephyr/drivers/rtc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/mem_stats.h>
#include <cstdint>
 
namespace monitor {
constexpr uint32_t kTopMaxThreads = 20;
constexpr uint32_t kTopVisibleThreads = 8;
constexpr uint32_t kBarWidth = 30;
 
struct ThreadRow {
	k_tid_t tid;
	const char *name;
	int prio;
	uint32_t stack_free;
	uint64_t delta_cycles;
};
 
struct TopSnapshot {
	ThreadRow rows[kTopMaxThreads];
	uint32_t rows_count;
	uint32_t total_threads_seen;
	uint32_t unknown_stack;
	uint32_t min_free_stack;
	uint64_t delta_sum;
	int load_permille;
	uint32_t uptime_s;
	bool rtc_ok;
	struct rtc_time rtc_now;
	bool heap_ok;
	struct sys_memory_stats heap_stats;
	bool total_cycles_ok;
	k_thread_runtime_stats_t total_rt;
};
} // namespace monitor