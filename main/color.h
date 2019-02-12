#ifndef COLOR_H
#define COLOR_H
#include <stdint.h>

typedef union {
	struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
	} fields;
	uint32_t bits;
} rgb_color;

typedef union {
	struct {
		uint8_t h;
		uint8_t s;
		uint8_t v;
	} fields;
	uint32_t bits;
} hsv_color;

rgb_color hsv_to_rgb(hsv_color color);

/* helper functions colors can be initialized in expressions */
rgb_color rgb(uint8_t r, uint8_t g, uint8_t b);
hsv_color hsv(uint8_t h, uint8_t s, uint8_t v);

#endif /* COLOR_H */
