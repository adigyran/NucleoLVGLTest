#include "oled_demo.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <cstdint>
#include <string.h>
 
LOG_MODULE_REGISTER(oled_demo, LOG_LEVEL_INF);
 
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static struct display_capabilities display_caps;
static bool oled_ready;
static uint8_t framebuf[2048];
 
static void draw_pixel(uint16_t x, uint16_t y, bool on)
{
	size_t idx = x + ((size_t)(y >> 3) * display_caps.x_resolution);
	uint8_t bit = BIT(y & 0x7U);
 
	if (on) {
		framebuf[idx] |= bit;
	} else {
		framebuf[idx] &= (uint8_t)~bit;
	}
}
 
void oled_demo_init()
{
	if (!device_is_ready(display_dev)) {
		LOG_WRN("OLED device is not ready");
		return;
	}
 
	display_get_capabilities(display_dev, &display_caps);
	if ((display_caps.x_resolution * display_caps.y_resolution / 8U) > sizeof(framebuf)) {
		LOG_WRN("OLED frame buffer is too small for %ux%u",
			display_caps.x_resolution, display_caps.y_resolution);
		return;
	}
 
	if (display_set_pixel_format(display_dev, PIXEL_FORMAT_MONO10) != 0) {
		(void)display_set_pixel_format(display_dev, PIXEL_FORMAT_MONO01);
	}
 
	oled_ready = true;
	LOG_INF("OLED initialized: %ux%u", display_caps.x_resolution, display_caps.y_resolution);
}
 
void oled_demo_tick(uint32_t tick)
{
	struct display_buffer_descriptor desc;
	uint16_t width;
	uint16_t height;
	uint16_t bar_x;
 
	if (!oled_ready) {
		return;
	}
 
	width = display_caps.x_resolution;
	height = display_caps.y_resolution;
	bar_x = (uint16_t)(tick % width);
	memset(framebuf, 0, sizeof(framebuf));
 
	for (uint16_t y = 0; y < height; ++y) {
		for (uint16_t x = 0; x < width; ++x) {
			bool border = (x == 0U) || (x == (width - 1U)) || (y == 0U) || (y == (height - 1U));
			bool bar = (x == bar_x) || (x == ((bar_x + 1U) % width));
			bool stripe = ((x + y + tick) % 16U) == 0U;
			if (border || bar || stripe) {
				draw_pixel(x, y, true);
			}
		}
	}
 
	desc.buf_size = (width * height) / 8U;
	desc.width = width;
	desc.height = height;
	desc.pitch = width;
	desc.frame_incomplete = false;
 
	(void)display_write(display_dev, 0, 0, &desc, framebuf);
}