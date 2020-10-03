#ifndef PPM_H
#define PPM_H

#include <stdint.h>
#include <stddef.h>
#include "color.h"

#define PPM_MAGIC_NUMBER_1 'p'
#define PPM_MAGIC_NUMBER_2 '6'

typedef struct {
	unsigned int width;
	unsigned int height;
	uint16_t maxval;
	rgb_color* pixels;
} fp_ppm_image;

fp_ppm_image fp_parse_ppm(char* bytes, size_t length);

/* fp_ppm_image fp_create_image_frame */


#endif /* PPM_H */
