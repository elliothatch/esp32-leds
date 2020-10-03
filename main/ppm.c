#include "ppm.h"
#include "pixel.h"
#include "color.h"

/** returns pointer to the next non-comment char.
 * if there is a comment at the character, scans through until it finds a new line '\n' or hits the end of the string (indicated by "end"), then returns a pointer to the next char */
char* skip_comment(char* str, char* end) {
	if(str[0] != '#') {
		return str;
	}

	size_t i = 1;
	while(str[i] != '\n' && str+i+1 < end) {
		i++;
	}

	return str+i+1;
}


fp_ppm_image fp_parse_ppm(char* bytes, size_t length) {
	// TODO: check length overflow at every step

	fp_ppm_image image;

	char* currentChar = bytes;
	char* bytesEnd = bytes + length;
	currentChar = skip_comment(bytes, bytesEnd);

	if(currentChar[0] != PPM_MAGIC_NUMBER_1 && currentChar[1] != PPM_MAGIC_NUMBER_2) {
		printf("wrong file type\n");
		// error: wrong file type
	}

	currentChar += 2;
	currentChar = skip_comment(bytes, bytesEnd);
	image.width = strtol(currentChar, &currentChar, 10);
	currentChar = skip_comment(bytes, bytesEnd);
	image.height = strtol(currentChar, &currentChar, 10);
	currentChar = skip_comment(bytes, bytesEnd);
	image.maxval = strtol(currentChar, &currentChar, 10);
	currentChar = skip_comment(bytes, bytesEnd);

	printf("%d %d %d\n ", image.width, image.height, image.maxval);

	// TODO: normalize maxval, support higher maxval
	if(image.maxval >= 256) {
		printf("only single-byte ppm supported, use maxval smaller than 256\n");
	}

	image.pixels = (rgb_color*)currentChar;

	/* image.pixels = malloc(image.width * image.height * sizeof(rgb_color)); */
	/* memcpy(image.pixels, currentChar, bytesEnd - currentChar); */

	return image;
}

/* fp_frameid fp_create_image_frame() { */
/* 	fp_frameid frameId = fp_create_frame(width, height, rgb(0,0,0)); */
/* } */
