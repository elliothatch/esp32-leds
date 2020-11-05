#include <math.h>
#include <stdio.h>

#include "color.h"

#define HSV_SECTION_3 0x40

const uint8_t gamma8[] = {    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

rgb_color hsv_to_rgb(hsv_color color) {
	const int h = (color.fields.h * 192) >> 8;

     /* The brightness floor is minimum number that all of R, G, and B will be set to. */
    const int invsat = 255 - color.fields.s;
    const int brightness_floor = (color.fields.v * invsat) / 256;

    /* The color amplitude is the maximum amount of R, G, and B
     that will be added on top of the brightness_floor to
     create the specific hue desired. */
    const int color_amplitude = color.fields.v - brightness_floor;

    /* figure out which section of the hue wheel we're in,
     and how far offset we are within that section */
    const int section = h / HSV_SECTION_3; /* 0..2 */
    const int offset = h % HSV_SECTION_3; /* 0..63; */

    const int rampup = offset;
    const int rampdown = (HSV_SECTION_3 - 1) - offset;

    /* compute color-amplitude-scaled-down versions of rampup and rampdown */
    const int rampup_amp_adj = (rampup * color_amplitude) / (256 / 4);
    const int rampdown_amp_adj = (rampdown * color_amplitude) / (256 / 4);

    /* add brightness_floor offset to everything */
    const int rampup_adj_with_floor = rampup_amp_adj + brightness_floor;
    const int rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;

    rgb_color result;

    if(section) {
        if(section == 1) {
            /* section 1: 0x40..0x7F */
            result.fields.r = brightness_floor;
            result.fields.g = rampdown_adj_with_floor;
            result.fields.b = rampup_adj_with_floor;
        }
        else {
            /* section 2; 0x80..0xBF */
            result.fields.r = rampup_adj_with_floor;
            result.fields.g = brightness_floor;
            result.fields.b = rampdown_adj_with_floor;
        }
    }
    else {
        /* section 0: 0x00..0x3F */
        result.fields.r = rampdown_adj_with_floor;
        result.fields.g = rampup_adj_with_floor;
        result.fields.b = brightness_floor;
    }

    return result;
}

rgb_color rgb(uint8_t r, uint8_t g, uint8_t b) {
    rgb_color color = {.fields={ b, r, g }};
    return color;
}

rgb_color rgbMap(uint16_t index, uint8_t alpha) {
    rgb_color color = {.mapFields={ index, alpha }};
    return color;
}

hsv_color hsv(uint8_t h, uint8_t s, uint8_t v) {
    hsv_color color = {.fields={ h, s, v }};
    return color;
}

rgb_color rgb_add(rgb_color a, rgb_color b) {
	rgb_color color = {{
		fmin(a.fields.b + b.fields.b, 255),
		fmin(a.fields.r + b.fields.r, 255),
		fmin(a.fields.g + b.fields.g, 255),
	}};
	return color;
}

/** alpha values are used to interpolate each color field between 0 and its value, allowing "partial application" of multiply */
rgb_color rgb_addb(rgb_color a, uint8_t aAlpha, rgb_color b, uint8_t bAlpha) {
	rgb_color color = {{
		fmin(aAlpha*a.fields.b/255 + bAlpha*b.fields.b/255, 255),
		fmin(aAlpha*a.fields.r/255 + bAlpha*b.fields.r/255, 255),
		fmin(aAlpha*a.fields.g/255 + bAlpha*b.fields.g/255, 255),
	}};
	return color;
}

/** alpha values are used to interpolate each color field between 255 and its value, allowing "partial application" of multiply
 * only one alpha value should be less than 255, or you will experience "brightening" instead. this is because an alpha value of 0 means the color acts as white during the multiply
 */
rgb_color rgb_multiplyb(rgb_color a, uint8_t aAlpha, rgb_color b, uint8_t bAlpha) {
	rgb_color color = {{
		fmin((255 - (255 - a.fields.b)*aAlpha/255) * (255 - (255 - b.fields.b)*bAlpha/255) / 255, 255),
		fmin((255 - (255 - a.fields.r)*aAlpha/255) * (255 - (255 - b.fields.r)*bAlpha/255) / 255, 255),
		fmin((255 - (255 - a.fields.g)*aAlpha/255) * (255 - (255 - b.fields.g)*bAlpha/255) / 255, 255),
	}};
	return color;
}

rgb_color rgb_multiply(rgb_color a, rgb_color b) {
	rgb_color color = {{
		a.fields.b * b.fields.b / 255,
		a.fields.r * b.fields.r / 255,
		a.fields.g * b.fields.g / 255,
	}};
	return color;
}

rgb_color rgb_alpha(rgb_color a, uint8_t aAlpha, rgb_color b, uint8_t bAlpha) {
	uint8_t outAlpha = aAlpha + bAlpha * (255 - aAlpha) / 255;
	rgb_color color = {{
		(a.fields.b * aAlpha / 255 + b.fields.b * bAlpha / 255 * (255 - aAlpha) / 255) * 255 / outAlpha,
		(a.fields.r * aAlpha / 255 + b.fields.r * bAlpha / 255 * (255 - aAlpha) / 255) * 255 / outAlpha,
		(a.fields.g * aAlpha / 255 + b.fields.g * bAlpha / 255 * (255 - aAlpha) / 255) * 255 / outAlpha,
	}};
	return color;
}
