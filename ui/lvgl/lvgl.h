/* Совместимостный шим: SquareLine Studio 1.6.x генерирует #include "lvgl/lvgl.h",
 * а Zephyr предоставляет LVGL через <lvgl.h>. Этот файл перенаправляет включение.
 * Не удалять — перезаписывается только экспортом из SquareLine Studio, шим — нет.
 */
#include <lvgl.h>
