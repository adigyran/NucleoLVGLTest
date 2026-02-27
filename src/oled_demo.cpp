#include "oled_demo.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <errno.h>
#include <cstdint>
#include <stdlib.h>
#include <string.h>
 
LOG_MODULE_REGISTER(oled_demo, LOG_LEVEL_INF);
 
#define OLED_COLOR_ORDER_RGB 0
#define OLED_COLOR_ORDER_RBG 1
#define OLED_COLOR_ORDER_BRG 2
/* Service override for this panel revision. */
#define OLED_COLOR_ORDER OLED_COLOR_ORDER_BRG
 
static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static struct display_capabilities display_caps;
static bool oled_ready;
static uint16_t framebuf[128 * 128];
static uint8_t oled_brightness = 255;
 
enum oled_test_mode {
	OLED_TEST_ANIM = 0,
	OLED_TEST_RED,
	OLED_TEST_GREEN,
	OLED_TEST_BLUE,
	OLED_TEST_WHITE,
	OLED_TEST_BLACK,
	OLED_TEST_OFF,
};
 
static enum oled_test_mode test_mode = OLED_TEST_ANIM;
 
static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
	#if OLED_COLOR_ORDER == OLED_COLOR_ORDER_RGB
	return (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | ((b & 0xF8U) >> 3));
	#elif OLED_COLOR_ORDER == OLED_COLOR_ORDER_RBG
	return (uint16_t)(((r & 0xF8U) << 8) | ((b & 0xFCU) << 3) | ((g & 0xF8U) >> 3));
	#else
	/* This panel variant maps channels as B-R-G in 565 transfer order. */
	return (uint16_t)(((b & 0xF8U) << 8) | ((r & 0xFCU) << 3) | ((g & 0xF8U) >> 3));
	#endif
}
 
void oled_demo_init()
{
	if (!device_is_ready(display_dev)) {
		LOG_WRN("OLED device is not ready");
		return;
	}
 
	display_get_capabilities(display_dev, &display_caps);
	if ((display_caps.x_resolution * display_caps.y_resolution) > ARRAY_SIZE(framebuf)) {
		LOG_WRN("OLED frame buffer is too small for %ux%u",
			display_caps.x_resolution, display_caps.y_resolution);
		return;
	}
 
	if (display_set_pixel_format(display_dev, PIXEL_FORMAT_RGB_565) != 0) {
		LOG_WRN("RGB565 pixel format is not supported");
		return;
	}
	(void)display_set_contrast(display_dev, oled_brightness);
 
	oled_ready = true;
	LOG_INF("RGB OLED initialized: %ux%u", display_caps.x_resolution, display_caps.y_resolution);
}
 
void oled_demo_tick(uint32_t tick)
{
	struct display_buffer_descriptor desc = {};
	uint16_t width;
	uint16_t height;
	uint16_t bar_x;
 
	if (!oled_ready) {
		return;
	}
 
	width = display_caps.x_resolution;
	height = display_caps.y_resolution;
	bar_x = (uint16_t)(tick % width);
 
	if (test_mode == OLED_TEST_OFF) {
		return;
	}
 
	if (test_mode != OLED_TEST_ANIM) {
		uint16_t color = rgb565(0, 0, 0);
		if (test_mode == OLED_TEST_RED) {
			color = rgb565(255, 0, 0);
		} else if (test_mode == OLED_TEST_GREEN) {
			color = rgb565(0, 255, 0);
		} else if (test_mode == OLED_TEST_BLUE) {
			color = rgb565(0, 0, 255);
		} else if (test_mode == OLED_TEST_WHITE) {
			color = rgb565(255, 255, 255);
		}
		for (size_t i = 0; i < ARRAY_SIZE(framebuf); ++i) {
			framebuf[i] = color;
		}
	} else {
		memset(framebuf, 0, sizeof(framebuf));
 
		for (uint16_t y = 0; y < height; ++y) {
			for (uint16_t x = 0; x < width; ++x) {
				size_t idx = x + ((size_t)y * width);
				bool border = (x == 0U) || (x == (width - 1U)) || (y == 0U) || (y == (height - 1U));
				bool bar = (x == bar_x) || (x == ((bar_x + 1U) % width));
				uint8_t r = (uint8_t)((x * 2U + tick) & 0xFFU);
				uint8_t g = (uint8_t)((y * 2U + tick) & 0xFFU);
				uint8_t b = (uint8_t)(((x + y) + tick * 3U) & 0xFFU);
				uint16_t color = rgb565(r, g, b);
				if (border) {
					color = rgb565(255, 255, 255);
				}
				if (bar) {
					color = rgb565(255, 64, 0);
				}
				framebuf[idx] = color;
			}
		}
	}
 
	desc.buf_size = width * height * sizeof(uint16_t);
	desc.width = width;
	desc.height = height;
	desc.pitch = width;
	desc.frame_incomplete = false;
 
	(void)display_write(display_dev, 0, 0, &desc, framebuf);
}
 
static int cmd_oled_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_print(sh, "ready=%d mode=%d brightness=%u res=%ux%u",
		    oled_ready ? 1 : 0, (int)test_mode,
		    oled_brightness,
		    display_caps.x_resolution, display_caps.y_resolution);
	return 0;
}
 
static int cmd_oled_anim(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	test_mode = OLED_TEST_ANIM;
	shell_print(sh, "OLED mode: animation");
	return 0;
}
 
static int cmd_oled_off(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	test_mode = OLED_TEST_OFF;
	shell_print(sh, "OLED mode: off");
	return 0;
}
 
static int cmd_oled_solid(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: oled solid <r|g|b|w|k>");
		return -EINVAL;
	}
 
	switch (argv[1][0]) {
	case 'r':
		test_mode = OLED_TEST_RED;
		break;
	case 'g':
		test_mode = OLED_TEST_GREEN;
		break;
	case 'b':
		test_mode = OLED_TEST_BLUE;
		break;
	case 'w':
		test_mode = OLED_TEST_WHITE;
		break;
	case 'k':
		test_mode = OLED_TEST_BLACK;
		break;
	default:
		shell_error(sh, "Unknown color");
		return -EINVAL;
	}
 
	shell_print(sh, "OLED mode: solid");
	return 0;
}
 
static int cmd_oled_brightness(const struct shell *sh, size_t argc, char **argv)
{
	char *end = NULL;
	long val;
 
	if (argc != 2) {
		shell_error(sh, "Usage: oled brightness <0..255>");
		return -EINVAL;
	}
 
	val = strtol(argv[1], &end, 10);
	if ((end == argv[1]) || (*end != '\0') || (val < 0) || (val > 255)) {
		shell_error(sh, "Brightness must be 0..255");
		return -EINVAL;
	}
 
	oled_brightness = (uint8_t)val;
	if (oled_ready) {
		(void)display_set_contrast(display_dev, oled_brightness);
	}
	shell_print(sh, "OLED brightness: %u", oled_brightness);
	return 0;
}
 
SHELL_STATIC_SUBCMD_SET_CREATE(sub_oled,
	SHELL_CMD(status, NULL, "OLED status", cmd_oled_status),
	SHELL_CMD(anim, NULL, "OLED animation test", cmd_oled_anim),
	SHELL_CMD(off, NULL, "OLED off", cmd_oled_off),
	SHELL_CMD(brightness, NULL, "OLED brightness 0..255", cmd_oled_brightness),
	SHELL_CMD(solid, NULL, "OLED solid color (r|g|b|w|k)", cmd_oled_solid),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(oled, &sub_oled, "OLED test commands", NULL);