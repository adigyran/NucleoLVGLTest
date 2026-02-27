#include "top_collector.hpp"
#include "../rtc_service.hpp"
#include <zephyr/debug/cpu_load.h>
#include <zephyr/kernel.h>
#include <sys_malloc.h>
#include <cstdint>
 
namespace monitor {
struct PrevCycles {
	k_tid_t tid;
	uint64_t total_cycles;
};
 
static PrevCycles prev[kTopMaxThreads];
static uint32_t prev_count;
 
static uint64_t get_prev_cycles(k_tid_t tid)
{
	for (uint32_t i = 0; i < prev_count; ++i) {
		if (prev[i].tid == tid) {
			return prev[i].total_cycles;
		}
	}
	return 0;
}
 
static void set_prev_cycles(k_tid_t tid, uint64_t total_cycles)
{
	for (uint32_t i = 0; i < prev_count; ++i) {
		if (prev[i].tid == tid) {
			prev[i].total_cycles = total_cycles;
			return;
		}
	}
	if (prev_count < kTopMaxThreads) {
		prev[prev_count].tid = tid;
		prev[prev_count].total_cycles = total_cycles;
		++prev_count;
	}
}
 
static void sort_rows_by_delta(ThreadRow *rows, uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i) {
		for (uint32_t j = i + 1U; j < count; ++j) {
			if (rows[j].delta_cycles > rows[i].delta_cycles) {
				ThreadRow tmp = rows[i];
				rows[i] = rows[j];
				rows[j] = tmp;
			}
		}
	}
}
 
static void collect_thread_stats(const struct k_thread *thread, void *user_data)
{
	auto *snap = static_cast<TopSnapshot *>(user_data);
	auto *row = &snap->rows[snap->rows_count];
	k_thread_runtime_stats_t rt = {0};
	size_t stack_free = 0;
 
	snap->total_threads_seen++;
	if (snap->rows_count >= kTopMaxThreads) {
		return;
	}
 
	row->tid = (k_tid_t)thread;
	row->name = k_thread_name_get((k_tid_t)thread);
	row->prio = k_thread_priority_get((k_tid_t)thread);
 
	if (k_thread_stack_space_get(thread, &stack_free) == 0) {
		row->stack_free = (uint32_t)stack_free;
		if (row->stack_free < snap->min_free_stack) {
			snap->min_free_stack = row->stack_free;
		}
	} else {
		row->stack_free = 0;
		snap->unknown_stack++;
	}
 
	if (k_thread_runtime_stats_get((k_tid_t)thread, &rt) == 0) {
		uint64_t total = rt.total_cycles;
		row->delta_cycles = total - get_prev_cycles((k_tid_t)thread);
		set_prev_cycles((k_tid_t)thread, total);
	} else {
		row->delta_cycles = 0;
	}
 
	snap->rows_count++;
}
 
void collect_top_snapshot(TopSnapshot *out)
{
	if (out == nullptr) {
		return;
	}
 
	*out = {
		.rows_count = 0,
		.total_threads_seen = 0,
		.unknown_stack = 0,
		.min_free_stack = UINT32_MAX,
		.delta_sum = 0,
		.load_permille = cpu_load_get(true),
		.uptime_s = static_cast<uint32_t>(k_uptime_get() / 1000),
		.rtc_ok = false,
		.rtc_now = {},
		.heap_ok = false,
		.heap_stats = {},
		.total_cycles_ok = false,
		.total_rt = {},
	};
 
	if (out->load_permille < 0) {
		out->load_permille = 0;
	}
 
	out->rtc_ok = rtc_service_get(&out->rtc_now);
	k_thread_foreach(collect_thread_stats, out);
	sort_rows_by_delta(out->rows, out->rows_count);
 
	for (uint32_t i = 0; i < out->rows_count; ++i) {
		out->delta_sum += out->rows[i].delta_cycles;
	}
	if (out->min_free_stack == UINT32_MAX) {
		out->min_free_stack = 0;
	}
 
	out->heap_ok = malloc_runtime_stats_get(&out->heap_stats) == 0;
	out->total_cycles_ok = k_thread_runtime_stats_all_get(&out->total_rt) == 0;
}
} // namespace monitor