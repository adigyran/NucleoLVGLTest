#pragma once
#include <cstdint>

struct shell;

/* Управляет LVGL-экраном: инициализация SLS-UI, обновление виджетов.
 * Используется как синглтон — один дисплей, один экземпляр. */
class LvglDemo {
public:
    /* Доступ к единственному экземпляру. */
    static LvglDemo &instance();

    /* Инициализирует дисплей и SLS-UI. Вызывать один раз при старте. */
    void init();

    /* Вызывается каждые ~16 мс из главного потока. */
    void tick(uint32_t tick_ms);

    /* Устанавливает цвет фона экрана (RGB hex, напр. 0x04080f). */
    void set_bg_color(uint32_t rgb_hex);

    /* Выводит текущую статистику в Shell. */
    void print_stats(const struct shell *sh) const;

private:
    LvglDemo() = default;

    /* Настраивает начальные значения SLS-виджетов после ui_init(). */
    void setup_widgets();

    /* Считывает загрузку CPU, возвращает проценты (0..100). */
    uint8_t sample_cpu();

    /* Обновляет ui_Arc1 и ui_Label1 под lvgl_lock. */
    void update_widgets(uint8_t cpu_pct, uint16_t fps);

    bool     ready_        {false};
    uint32_t fps_count_    {0};
    uint32_t fps_last_ms_  {0};
    uint16_t fps_current_  {0};
    uint16_t cpu_permille_ {0};
    uint32_t bg_color_     {0x04080f};
};

/* C-совместимый API: вызывается из app_main.cpp. */
void lvgl_demo_init();
void lvgl_demo_tick(uint32_t tick_ms);
