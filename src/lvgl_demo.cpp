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

/* Перекрашивает виджеты после изменения калибровки */
static void apply_colors()
{
	if (!demo_ready) {
		return;
	}
	lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(cal_bg), 0);
	lv_obj_set_style_bg_color(anim_bar, lv_color_hex(cal_accent), LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(arc_left,  lv_color_hex(cal_arc_l), LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(arc_right, lv_color_hex(cal_arc_r), LV_PART_INDICATOR);
}

/* ---------- инициализация -------------------------------------------------- */
void lvgl_demo_init()
{
	if (!device_is_ready(display_dev)) {
		LOG_WRN("Display device is not ready");
		return;
	}

	lv_obj_t *screen = lv_scr_act();

	/* Фон */
	lv_obj_set_style_bg_color(screen, lv_color_hex(cal_bg), 0);

	/* Заголовок */
	lv_obj_t *title = lv_label_create(screen);
	lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
	lv_label_set_text(title, "Hello, LVGL!");
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

	/* Подпись */
	lv_obj_t *sub = lv_label_create(screen);
	lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
	lv_obj_set_style_text_color(sub, lv_color_hex(0x6fcfeb), 0);
	lv_label_set_text(sub, "STM32H743");
	lv_obj_align_to(sub, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

	/* Левая дуга — большая, 360° */
	arc_left = lv_arc_create(screen);
	lv_obj_set_size(arc_left, 72, 72);
	lv_obj_align(arc_left, LV_ALIGN_CENTER, -22, 8);
	lv_arc_set_range(arc_left, 0, 99);
	lv_arc_set_bg_angles(arc_left, 0, 360);
	lv_obj_set_style_arc_width(arc_left, 7, LV_PART_MAIN);
	lv_obj_set_style_arc_width(arc_left, 7, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(arc_left, lv_color_hex(0x0d2038), LV_PART_MAIN);
	lv_obj_set_style_arc_color(arc_left, lv_color_hex(cal_arc_l), LV_PART_INDICATOR);
	lv_obj_clear_flag(arc_left, LV_OBJ_FLAG_CLICKABLE);

	/* Правая дуга — малая, 270° */
	arc_right = lv_arc_create(screen);
	lv_obj_set_size(arc_right, 42, 42);
	lv_obj_align(arc_right, LV_ALIGN_CENTER, 30, 8);
	lv_arc_set_range(arc_right, 0, 99);
	lv_arc_set_bg_angles(arc_right, 135, 45);
	lv_obj_set_style_arc_width(arc_right, 6, LV_PART_MAIN);
	lv_obj_set_style_arc_width(arc_right, 6, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(arc_right, lv_color_hex(0x1a1012), LV_PART_MAIN);
	lv_obj_set_style_arc_color(arc_right, lv_color_hex(cal_arc_r), LV_PART_INDICATOR);
	lv_obj_clear_flag(arc_right, LV_OBJ_FLAG_CLICKABLE);

	/* Центральный прямоугольник с закруглёнными углами */
	pulse_box = lv_obj_create(screen);
	lv_obj_set_size(pulse_box, 36, 36);
	lv_obj_align(pulse_box, LV_ALIGN_CENTER, -22, 8);
	lv_obj_set_style_radius(pulse_box, 8, 0);
	lv_obj_set_style_bg_color(pulse_box, lv_color_hex(0x0d2038), 0);
	lv_obj_set_style_border_width(pulse_box, 0, 0);
	lv_obj_set_style_pad_all(pulse_box, 0, 0);
	lv_obj_clear_flag(pulse_box, LV_OBJ_FLAG_SCROLLABLE);

	/* Значение внутри */
	val_label = lv_label_create(pulse_box);
	lv_obj_set_style_text_font(val_label, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(val_label, lv_color_hex(cal_arc_l), 0);
	lv_label_set_text(val_label, "0");
	lv_obj_center(val_label);

	/* Нижняя зона (от y=92): CPU + FPS
	 *
	 *  [CPU]  [====CPU BAR=====]  [animBar]
	 *              XX% | XX FPS
	 */

	/* Подпись "CPU" */
	cpu_label = lv_label_create(screen);
	lv_obj_set_style_text_font(cpu_label, &lv_font_montserrat_12, 0);
	lv_obj_set_style_text_color(cpu_label, lv_color_hex(0x5a8a6a), 0);
	lv_label_set_text(cpu_label, "CPU");
	lv_obj_align(cpu_label, LV_ALIGN_BOTTOM_LEFT, 4, -28);

	/* Бар CPU-загрузки (0..1000 permille) */
	cpu_bar = lv_bar_create(screen);
	lv_obj_set_size(cpu_bar, 80, 7);
	lv_obj_align(cpu_bar, LV_ALIGN_BOTTOM_LEFT, 30, -26);
	lv_bar_set_range(cpu_bar, 0, 1000);
	lv_obj_set_style_bg_color(cpu_bar, lv_color_hex(0x0d2038), LV_PART_MAIN);
	lv_obj_set_style_bg_color(cpu_bar, lv_color_hex(0x21eb8a), LV_PART_INDICATOR);
	lv_obj_set_style_radius(cpu_bar, 3, LV_PART_MAIN);
	lv_obj_set_style_radius(cpu_bar, 3, LV_PART_INDICATOR);

	/* Статусная строка: "XX% | XX FPS" */
	status_label = lv_label_create(screen);
	lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
	lv_obj_set_style_text_color(status_label, lv_color_hex(0x6a9a7a), 0);
	lv_label_set_text(status_label, "--% | -- FPS");
	lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -4);

	/* Маленький анимационный бар-демо (правый нижний угол) */
	anim_bar = lv_bar_create(screen);
	lv_obj_set_size(anim_bar, 14, 30);
	lv_obj_align(anim_bar, LV_ALIGN_BOTTOM_RIGHT, -4, -20);
	lv_bar_set_range(anim_bar, 0, 99);
	lv_obj_set_style_bg_color(anim_bar, lv_color_hex(0x0d2038), LV_PART_MAIN);
	lv_obj_set_style_bg_color(anim_bar, lv_color_hex(cal_accent), LV_PART_INDICATOR);
	lv_obj_set_style_radius(anim_bar, 3, LV_PART_MAIN);
	lv_obj_set_style_radius(anim_bar, 3, LV_PART_INDICATOR);

	(void)display_blanking_off(display_dev);
	demo_ready = true;
	LOG_INF("LVGL demo initialized (DMA2D + partial refresh + workqueue)");
}

/* ---------- тик-функция ---------------------------------------------------- */
/* Вызывается из main-треда каждые 16 мс.
 * lv_timer_handler() вызывается workqueue автоматически — здесь только
 * обновление значений виджетов под mutex. */
void lvgl_demo_tick(uint32_t tick_ms)
{
	if (!demo_ready) {
		return;
	}

	/* Треугольная волна 0..98..0 с периодом ~3 с */
	uint16_t phase = (uint16_t)((tick_ms / 15U) % 196U);
	uint16_t val   = (phase < 98U) ? phase : (uint16_t)(196U - phase);

	/* Только обновляем значение если оно изменилось */
	static uint16_t prev_val = 0xFFFFU;
	if (val == prev_val) {
		return;
	}
	prev_val = val;

	/* Блокируем LVGL на время обновления виджетов */
	/* Читаем CPU-загрузку до lock, чтобы не задерживать LVGL */
	int32_t load_raw = cpu_load_get(false);
	if (load_raw < 0) {
		load_raw = 0;
	}

	lvgl_lock();

	/* Анимационные виджеты */
	lv_bar_set_value(anim_bar, (int16_t)val, LV_ANIM_OFF);
	lv_arc_set_value(arc_left,  (int16_t)val);
	lv_arc_set_value(arc_right, (int16_t)(98U - val));

	/* Пульсация яркости центрального блока */
	uint8_t opa = (uint8_t)(150U + (uint8_t)(val * 105U / 98U));
	lv_obj_set_style_bg_opa(pulse_box, opa, 0);

	/* Число в центре — не чаще чем раз в 100 мс */
	if ((tick_ms % 100U) == 0U) {
		char num[8];
		(void)snprintf(num, sizeof(num), "%u", (unsigned int)val);
		lv_label_set_text(val_label, num);
	}

	/* CPU-бар + статусная строка — обновляем раз в 500 мс */
	fps_frame_count++;
	if ((tick_ms - fps_last_ms) >= 500U) {
		fps_current    = (uint16_t)(fps_frame_count * 2U);  /* *2 т.к. 500 мс */
		fps_frame_count = 0;
		fps_last_ms    = tick_ms;
		cpu_permille   = (uint16_t)load_raw;

		/* Обновляем CPU-бар */
		lv_bar_set_value(cpu_bar, (int16_t)cpu_permille, LV_ANIM_OFF);

		/* Цвет бара: зелёный < 50%, жёлтый < 80%, красный >= 80% */
		uint32_t bar_color;
		if (cpu_permille < 500U) {
			bar_color = 0x21eb8a;  /* зелёный */
		} else if (cpu_permille < 800U) {
			bar_color = 0xf5c518;  /* жёлтый */
		} else {
			bar_color = 0xff3c3c;  /* красный */
		}
		lv_obj_set_style_bg_color(cpu_bar, lv_color_hex(bar_color), LV_PART_INDICATOR);

		/* "XX% | XX FPS" */
		char text[24];
		(void)snprintf(text, sizeof(text), "%u.%u%% | %uFPS",
			(unsigned int)(cpu_permille / 10U),
			(unsigned int)(cpu_permille % 10U),
			(unsigned int)fps_current);
		lv_label_set_text(status_label, text);
	}

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
