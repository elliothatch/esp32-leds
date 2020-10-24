#ifndef PPM_H
#define PPM_H

#include <stdint.h>
#include <stddef.h>
#include "color.h"
#include "frame.h"

#define PPM_MAGIC_NUMBER_1 'p'
#define PPM_MAGIC_NUMBER_2 '6'

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} fp_ppm_rgb;

typedef struct {
	unsigned int width;
	unsigned int height;
	uint16_t maxval;
	fp_ppm_rgb* pixels;
} fp_ppm_image;

fp_ppm_image fp_ppm_parse(char* bytes, size_t length);
fp_frameid fp_ppm_create_frame(char* bytes, size_t length);

/* fp_ppm_image fp_create_image_frame */


#endif /* PPM_H */
