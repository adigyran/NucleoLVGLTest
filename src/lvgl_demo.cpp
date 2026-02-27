/* LVGL-демо для SSD1351 128×128 на STM32H743.
 * Отображает загрузку CPU и FPS на экране, построенном в SquareLine Studio.
 * Shell-команда `oled` позволяет менять цвет фона и смотреть статистику.
 */
#include "lvgl_demo.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/debug/cpu_load.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <lvgl.h>
#include <lvgl_zephyr.h>
/* Зephyr собирается с -nostdinc++, C++-заголовки недоступны — используем C. */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "ui.h"
#include "rtc_service.hpp"

LOG_MODULE_REGISTER(lvgl_demo, LOG_LEVEL_INF);

/* ---- синглтон ------------------------------------------------------------ */

LvglDemo &LvglDemo::instance()
{
    static LvglDemo inst;
    return inst;
}

/* ---- private: настройка виджетов ----------------------------------------- */

void LvglDemo::setup_widgets()
{
    /* Явный локальный стиль фона имеет приоритет над тинтом темы SLS. */
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(bg_color_), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen1, LV_OPA_COVER, LV_STATE_DEFAULT);

    /* Диапазон дуги: 0..100 (проценты CPU). */
    lv_arc_set_range(ui_Arc1, 0, 100);
    lv_arc_set_value(ui_Arc1, 0);

    /* Начальные значения виджетов. */
    lv_label_set_text(ui_lCpu,  "--%");
    lv_label_set_text(ui_lTime, "--:--");
    lv_label_set_text(ui_timel, "--");
}

/* ---- private: замер CPU -------------------------------------------------- */

uint8_t LvglDemo::sample_cpu()
{
    int32_t raw = cpu_load_get(false);
    if (raw < 0) {
        raw = 0;
    }
    cpu_permille_ = static_cast<uint16_t>(raw);
    return static_cast<uint8_t>(cpu_permille_ / 10U);  /* permille → percent */
}

/* ---- private: обновление виджетов ---------------------------------------- */

void LvglDemo::update_widgets(uint8_t cpu_pct, uint16_t fps)
{
    ARG_UNUSED(fps);

    /* Читаем время из RTC до захвата мьютекса LVGL. */
    struct rtc_time t = {};
    const bool has_time = rtc_service_get(&t);

    char cpu_buf[8];
    (void)snprintf(cpu_buf, sizeof(cpu_buf), "%u%%",
        static_cast<unsigned>(cpu_pct));

    char hhmm_buf[8];
    char sec_buf[4];
    if (has_time) {
        (void)snprintf(hhmm_buf, sizeof(hhmm_buf), "%02d:%02d",
            t.tm_hour, t.tm_min);
        (void)snprintf(sec_buf, sizeof(sec_buf), "%02d", t.tm_sec);
    } else {
        (void)snprintf(hhmm_buf, sizeof(hhmm_buf), "--:--");
        (void)snprintf(sec_buf,  sizeof(sec_buf),  "--");
    }

    lvgl_lock();
    lv_arc_set_value(ui_Arc1,  cpu_pct);
    lv_label_set_text(ui_lCpu,  cpu_buf);
    lv_label_set_text(ui_lTime, hhmm_buf);
    lv_label_set_text(ui_timel, sec_buf);
    lvgl_unlock();
}

/* ---- public: init --------------------------------------------------------- */

void LvglDemo::init()
{
    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(disp)) {
        LOG_WRN("Display device is not ready");
        return;
    }

    lvgl_lock();
    ui_init();
    setup_widgets();
    lvgl_unlock();

    (void)display_blanking_off(disp);
    ready_ = true;
    LOG_INF("LVGL demo initialized");
}

/* ---- public: tick --------------------------------------------------------- */

void LvglDemo::tick(uint32_t tick_ms)
{
    if (!ready_) {
        return;
    }

    fps_count_++;

    /* Обновляем виджеты раз в 500 мс. */
    if ((tick_ms - fps_last_ms_) < 500U) {
        return;
    }
    fps_current_  = static_cast<uint16_t>(fps_count_ * 2U);  /* *2 т.к. 500 мс */
    fps_count_    = 0;
    fps_last_ms_  = tick_ms;

    const uint8_t cpu_pct = sample_cpu();
    update_widgets(cpu_pct, fps_current_);
}

/* ---- public: set_bg_color ------------------------------------------------- */

void LvglDemo::set_bg_color(uint32_t rgb_hex)
{
    bg_color_ = rgb_hex;
    if (!ready_) {
        return;
    }
    lvgl_lock();
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(rgb_hex), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen1, LV_OPA_COVER, LV_STATE_DEFAULT);
    lvgl_unlock();
}

/* ---- public: print_stats -------------------------------------------------- */

void LvglDemo::print_stats(const struct shell *sh) const
{
    shell_print(sh, "ready      : %s",   ready_ ? "yes" : "no");
    shell_print(sh, "cpu        : %u.%u%%",
        static_cast<unsigned>(cpu_permille_ / 10U),
        static_cast<unsigned>(cpu_permille_ % 10U));
    shell_print(sh, "fps        : %u",   static_cast<unsigned>(fps_current_));
    shell_print(sh, "bg_color   : #%06x", static_cast<unsigned>(bg_color_));
}

/* ---- C API --------------------------------------------------------------- */

void lvgl_demo_init()               { LvglDemo::instance().init(); }
void lvgl_demo_tick(uint32_t ms)    { LvglDemo::instance().tick(ms); }

/* ---- Shell: oled bg RRGGBB / oled info ----------------------------------- */

static int cmd_oled_bg(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Использование: oled bg RRGGBB");
        return -EINVAL;
    }

    char *end = nullptr;
    errno = 0;
    unsigned long color = strtoul(argv[1], &end, 16);
    if (errno != 0 || *end != '\0' || color > 0xFFFFFFUL) {
        shell_error(sh, "Неверный цвет: 6 hex-цифр, напр. 04080f");
        return -EINVAL;
    }

    LvglDemo::instance().set_bg_color(static_cast<uint32_t>(color));
    shell_print(sh, "bg = #%06lx", color);
    return 0;
}

static int cmd_oled_info(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    LvglDemo::instance().print_stats(sh);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_oled,
    SHELL_CMD(bg,   NULL, "Цвет фона экрана (RRGGBB)", cmd_oled_bg),
    SHELL_CMD(info, NULL, "Статистика CPU/FPS",         cmd_oled_info),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(oled, &sub_oled, "OLED LVGL команды", NULL);
