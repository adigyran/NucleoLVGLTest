/* Демо-экран LVGL 128×128 для SSD1351 на STM32H743.
 * Анимации обновляются вручную в tick-функции — без lv_anim repeat/playback
 * (так избегаем зависания на границе 100%).
 * Shell-команды позволяют калибровать цвета на ходу через UART.
 */
#include "lvgl_demo.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/debug/cpu_load.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <lvgl.h>
#include <lvgl_zephyr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ui.h"

LOG_MODULE_REGISTER(lvgl_demo, LOG_LEVEL_INF);

/* ---------- состояние дисплея -------------------------------------------- */
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static bool demo_ready;

/* ---------- виджеты -------------------------------------------------------- */
static lv_obj_t *status_label;   /* нижняя строка: "XX% | XXFPS"         */
static lv_obj_t *cpu_bar;        /* бар CPU-загрузки                      */
static lv_obj_t *cpu_label;      /* "CPU" подпись слева от бара            */
static lv_obj_t *anim_bar;       /* анимированный прогресс (демо)          */
static lv_obj_t *arc_left;       /* левая дуга                             */
static lv_obj_t *arc_right;      /* правая дуга (в противофазе)            */
static lv_obj_t *pulse_box;      /* центральный цветной прямоугольник      */
static lv_obj_t *val_label;      /* значение в центре                      */

/* ---------- FPS + CPU ------------------------------------------------------- */
static uint32_t fps_frame_count;
static uint32_t fps_last_ms;
static uint16_t fps_current;
static uint16_t cpu_permille;

/* ---------- калибровка цвета ----------------------------------------------- */
/* remap-value контроллера задаётся в overlay; здесь только LVGL-цвета */
static uint32_t cal_accent = 0x21eb8a;   /* зелёный акцент прогресс-бара  */
static uint32_t cal_arc_l  = 0x2196ff;   /* цвет левой дуги               */
static uint32_t cal_arc_r  = 0xff9c2f;   /* цвет правой дуги              */
static uint32_t cal_bg     = 0x04080f;   /* цвет фона                     */

/* ---------- вспомогательные функции --------------------------------------- */

/* Перекрашивает виджеты после изменения калибровки.
 * Работает только с виджетами, созданными вручную (не SLS). */
static void apply_colors()
{
	if (!demo_ready) {
		return;
	}
	lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(cal_bg), 0);
	if (anim_bar)  { lv_obj_set_style_bg_color(anim_bar,  lv_color_hex(cal_accent), LV_PART_INDICATOR); }
	if (arc_left)  { lv_obj_set_style_arc_color(arc_left,  lv_color_hex(cal_arc_l), LV_PART_INDICATOR); }
	if (arc_right) { lv_obj_set_style_arc_color(arc_right, lv_color_hex(cal_arc_r), LV_PART_INDICATOR); }
}

/* ---------- инициализация -------------------------------------------------- */
void lvgl_demo_init()
{
	if (!device_is_ready(display_dev)) {
		LOG_WRN("Display device is not ready");
		return;
	}
	lvgl_lock();
	ui_init();
	/* Диапазон дуги: 0..100 (проценты CPU) */
	lv_arc_set_range(ui_Arc1, 0, 100);
	lv_arc_set_value(ui_Arc1, 0);
	lv_label_set_text(ui_Label1, "---%\n-- FPS");
	lvgl_unlock();
	(void)display_blanking_off(display_dev);
	demo_ready = true;
	LOG_INF("LVGL demo initialized (SquareLine Studio UI)");
}

/* ---------- тик-функция ---------------------------------------------------- */
/* Вызывается из main-треда каждые 16 мс.
 * Обновляет ui_Arc1 и ui_Label1 из ui_Screen1.h раз в 500 мс:
 *   ui_Arc1   → загрузка CPU в процентах (0..100)
 *   ui_Label1 → "XX%\nYY FPS"
 */
void lvgl_demo_tick(uint32_t tick_ms)
{
	if (!demo_ready) {
		return;
	}

	fps_frame_count++;

	/* Обновляем виджеты не чаще чем раз в 500 мс */
	if ((tick_ms - fps_last_ms) < 500U) {
		return;
	}
	fps_current     = (uint16_t)(fps_frame_count * 2U);   /* *2 т.к. период 500 мс */
	fps_frame_count = 0;
	fps_last_ms     = tick_ms;

	int32_t load_raw = cpu_load_get(false);
	if (load_raw < 0) {
		load_raw = 0;
	}
	cpu_permille = (uint16_t)load_raw;
	uint8_t cpu_pct = (uint8_t)(cpu_permille / 10U);  /* permille → percent */

	char buf[16];
	(void)snprintf(buf, sizeof(buf), "%u%%\n%u FPS",
		(unsigned int)cpu_pct, (unsigned int)fps_current);

	lvgl_lock();
	lv_arc_set_value(ui_Arc1, cpu_pct);
	lv_label_set_text(ui_Label1, buf);
	lvgl_unlock();
}

/* ========== Shell-команды калибровки ======================================
 *
 *   oled cal bg   RRGGBB   — цвет фона
 *   oled cal acc  RRGGBB   — цвет индикатора прогресс-бара
 *   oled cal arcl RRGGBB   — левая дуга / число в центре
 *   oled cal arcr RRGGBB   — правая дуга
 *   oled cal show          — вывести текущие значения
 *
 * Пример: oled cal acc 00ff88
 * ======================================================================= */

static int cmd_cal_set(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(sh, "Usage: oled cal <bg|acc|arcl|arcr> RRGGBB");
		return -EINVAL;
	}

	char *end = nullptr;
	errno = 0;
	unsigned long color = strtoul(argv[2], &end, 16);
	if (errno != 0 || *end != '\0' || color > 0xFFFFFFUL) {
		shell_error(sh, "Неверный цвет: должно быть RRGGBB (hex, 6 цифр)");
		return -EINVAL;
	}

	const char *name = argv[1];
	if      (!strcmp(name, "bg"))   { cal_bg    = (uint32_t)color; }
	else if (!strcmp(name, "acc"))  { cal_accent = (uint32_t)color; }
	else if (!strcmp(name, "arcl")) { cal_arc_l  = (uint32_t)color; }
	else if (!strcmp(name, "arcr")) { cal_arc_r  = (uint32_t)color; }
	else {
		shell_error(sh, "Неизвестное поле: bg | acc | arcl | arcr");
		return -EINVAL;
	}

	apply_colors();
	shell_print(sh, "ok: %s = #%06lx", name, color);
	return 0;
}

static int cmd_cal_show(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_print(sh, "bg   = #%06x", cal_bg);
	shell_print(sh, "acc  = #%06x", cal_accent);
	shell_print(sh, "arcl = #%06x", cal_arc_l);
	shell_print(sh, "arcr = #%06x", cal_arc_r);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_oled_cal,
	SHELL_CMD(set,  NULL, "oled cal set <bg|acc|arcl|arcr> RRGGBB", cmd_cal_set),
	SHELL_CMD(show, NULL, "Показать текущие калибровочные цвета",   cmd_cal_show),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_oled,
	SHELL_CMD(cal, &sub_oled_cal, "Калибровка цветов дисплея", NULL),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(oled, &sub_oled, "OLED LVGL команды", NULL);
