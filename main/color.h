#ifndef COLOR_H
#define COLOR_H
#include <stdint.h>

/** WS2812 use BRG order */
typedef union {
	struct {
		uint8_t b;
		uint8_t r;
		uint8_t g;
	} fields;
	uint32_t bits;
	
	/* represents a pixel mapping */
	struct {
		uint16_t index;
		uint8_t alpha;
	} mapFields;
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
rgb_color rgbMap(uint16_t index, uint8_t alpha);
hsv_color hsv(uint8_t h, uint8_t s, uint8_t v);

typedef rgb_color (*blend_fn)(rgb_color a, uint8_t aAlpha, rgb_color b, uint8_t bAlpha);

/* blending operations
 * "b" versions of blending operations support "partial" blending by applying alpha values as weights to the calculation.
 * they are added so they can be used in blending interfaces as a blend_fn. some combinations of alpha values make not always make sense
 */
rgb_color rgb_add(rgb_color a, rgb_color b);
rgb_color rgb_addb(rgb_color a, uint8_t aAlpha, rgb_color b, uint8_t bAlpha);

rgb_color rgb_multiply(rgb_color a, rgb_color b);
rgb_color rgb_multiplyb(rgb_color a, uint8_t aAlpha, rgb_color b, uint8_t bAlpha);

rgb_color rgb_alpha(rgb_color a, uint8_t aAlpha, rgb_color b, uint8_t bAlpha);

/** gamma correction lookup table */
extern const uint8_t gamma8[];


#endif /* COLOR_H */
