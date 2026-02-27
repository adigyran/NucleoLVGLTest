#include "top_renderer.hpp"
#include <zephyr/sys/printk.h>
#include <cstdint>
 
namespace monitor {
constexpr uint32_t kRowTitle = 1;
constexpr uint32_t kRowCpu = 2;
constexpr uint32_t kRowHeap = 3;
constexpr uint32_t kRowThr = 4;
constexpr uint32_t kRowCyc = 5;
constexpr uint32_t kRowHeader = 7;
constexpr uint32_t kRowThreadsStart = 8;
 
#define ANSI_RESET "\x1b[0m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_RED "\x1b[31m"
#define ANSI_CYAN "\x1b[36m"
 
static bool layout_drawn;
 
static void cursor_to(uint32_t row, uint32_t col)
{
	printk("\x1b[%u;%uH", (unsigned int)row, (unsigned int)col);
}
 
static void fill_bar(char *out, uint32_t width, uint32_t pct)
{
	uint32_t filled = (pct * width) / 100U;
	if (filled > width) {
		filled = width;
	}
	for (uint32_t i = 0; i < width; ++i) {
		out[i] = (i < filled) ? '#' : '-';
	}
	out[width] = '\0';
}
 
void draw_layout_once()
{
	if (layout_drawn) {
		return;
	}
 
	printk("\x1b[2J\x1b[H\x1b[?25l");
	cursor_to(kRowTitle, 1);
	printk(ANSI_CYAN "Zephyr TOP" ANSI_RESET "\n");
	printk("CPU\n");
	printk("HEAP\n");
	printk("THR\n");
	printk("CYC\n\n");
	printk("%-12s %-5s %-8s %-10s %-6s\n", "thread", "prio", "stack(B)", "delta", "load%");
	for (uint32_t i = 0; i < kTopVisibleThreads; ++i) {
		printk("\n");
	}
	layout_drawn = true;
}
 
void render_top_snapshot(const TopSnapshot *snap)
{
	char bar[kBarWidth + 1];
	uint32_t load_pct;
	const char *cpu_color;
	uint32_t top_n;
 
	if (snap == nullptr) {
		return;
	}
 
	load_pct = (uint32_t)(snap->load_permille / 10);
	cpu_color = (load_pct >= 80U) ? ANSI_RED : ((load_pct >= 50U) ? ANSI_YELLOW : ANSI_GREEN);
	top_n = (snap->rows_count < kTopVisibleThreads) ? snap->rows_count : kTopVisibleThreads;
 
	cursor_to(kRowTitle, 1);
	printk(ANSI_CYAN "Zephyr TOP" ANSI_RESET "  uptime:%us  ", snap->uptime_s);
	if (snap->rtc_ok) {
		printk("rtc:%04d-%02d-%02d %02d:%02d:%02d",
		       snap->rtc_now.tm_year + 1900, snap->rtc_now.tm_mon + 1, snap->rtc_now.tm_mday,
		       snap->rtc_now.tm_hour, snap->rtc_now.tm_min, snap->rtc_now.tm_sec);
	} else {
		printk("rtc:n/a");
	}
	printk("\x1b[K");
 
	fill_bar(bar, kBarWidth, load_pct);
	cursor_to(kRowCpu, 1);
	printk("%sCPU [%s] %d.%d%%%s", cpu_color, bar,
	       snap->load_permille / 10, snap->load_permille % 10, ANSI_RESET);
	printk("\x1b[K");
 
	cursor_to(kRowHeap, 1);
	if (snap->heap_ok) {
		auto heap_total = static_cast<uint32_t>(
			snap->heap_stats.free_bytes + snap->heap_stats.allocated_bytes);
		uint32_t heap_pct = (heap_total > 0U)
			? static_cast<uint32_t>((snap->heap_stats.allocated_bytes * 100U) / heap_total)
			: 0U;
		fill_bar(bar, kBarWidth, heap_pct);
		printk(ANSI_CYAN "HEAP" ANSI_RESET " [%s] used:%uB free:%uB peak:%uB",
		       bar,
		       (unsigned int)snap->heap_stats.allocated_bytes,
		       (unsigned int)snap->heap_stats.free_bytes,
		       (unsigned int)snap->heap_stats.max_allocated_bytes);
	} else {
		printk(ANSI_CYAN "HEAP" ANSI_RESET " [------------------------------] n/a");
	}
	printk("\x1b[K");
 
	cursor_to(kRowThr, 1);
	printk(ANSI_CYAN "THR " ANSI_RESET "total:%u shown:%u min_free_stack:%uB unknown_stack:%u",
	       (unsigned int)snap->total_threads_seen, (unsigned int)snap->rows_count,
	       (unsigned int)snap->min_free_stack, (unsigned int)snap->unknown_stack);
	printk("\x1b[K");
 
	cursor_to(kRowCyc, 1);
	if (snap->total_cycles_ok) {
		printk(ANSI_CYAN "CYC " ANSI_RESET "total_non_idle:%llu idle:%llu",
		       (unsigned long long)snap->total_rt.total_cycles,
		       (unsigned long long)snap->total_rt.idle_cycles);
	} else {
		printk(ANSI_CYAN "CYC " ANSI_RESET "n/a");
	}
	printk("\x1b[K");
 
	cursor_to(kRowHeader, 1);
	printk("%-12s %-5s %-8s %-10s %-6s", "thread", "prio", "stack(B)", "delta", "load%");
	printk("\x1b[K");
 
	for (uint32_t i = 0; i < top_n; ++i) {
		uint32_t pct = (snap->delta_sum > 0U)
			? static_cast<uint32_t>((snap->rows[i].delta_cycles * 100U) / snap->delta_sum)
			: 0U;
		const char *name = (snap->rows[i].name != nullptr) ? snap->rows[i].name : "(noname)";
		cursor_to(kRowThreadsStart + i, 1);
		printk("%-12s %-5d %-8u %-10llu %-6u",
		       name,
		       snap->rows[i].prio,
		       snap->rows[i].stack_free,
		       (unsigned long long)snap->rows[i].delta_cycles,
		       pct);
		printk("\x1b[K");
	}
	for (uint32_t i = top_n; i < kTopVisibleThreads; ++i) {
		cursor_to(kRowThreadsStart + i, 1);
		printk("\x1b[K");
	}
}
} // namespace monitor