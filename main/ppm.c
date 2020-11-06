#include "ppm.h"
#include "color.h"

#include "freertos/FreeRTOS.h"
#include <string.h>
#include <errno.h>

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


fp_ppm_image fp_ppm_parse(char* bytes, size_t length) {
	// TODO: check length overflow at every step

	fp_ppm_image image;

	char* currentChar = bytes;
	char* bytesEnd = bytes + length;
	
	if(currentChar[0] != PPM_MAGIC_NUMBER_1 && currentChar[1] != PPM_MAGIC_NUMBER_2) {
		printf("wrong file type\n");
		// error: wrong file type
	}

	currentChar += 3;

	currentChar = skip_comment(currentChar, bytesEnd);
	image.width = strtol(currentChar, &currentChar, 10);

	currentChar = skip_comment(currentChar, bytesEnd);
	image.height = strtol(currentChar, &currentChar, 10);

	currentChar = skip_comment(currentChar, bytesEnd);
	image.maxval = strtol(currentChar, &currentChar, 10);

	currentChar += 1;
	// TODO: normalize maxval, support higher maxval
	if(image.maxval >= 256) {
		printf("only single-byte ppm supported, use maxval smaller than 256\n");
	}

	image.pixels = (fp_ppm_rgb*)currentChar;

	return image;
}

fp_frameid fp_ppm_create_frame(char* bytes, size_t length) {
	fp_ppm_image image = fp_ppm_parse(bytes, length);

	fp_frameid frameid = fp_frame_create(image.width, image.height, rgb(0, 0, 0));
	fp_frame* frame = fp_frame_get(frameid);

	for(int i = 0; i < image.width * image.height; i++) {
		frame->pixels[i] = rgb(image.pixels[i].r, image.pixels[i].g, image.pixels[i].b);
	}

	return frameid;
}

fp_frameid fp_ppm_load_image(char* filepath) {
	FILE* file = fopen(filepath, "rb");
	if(!file) {
		printf("fp_ppm_load_image: error opening file (%s)\n", strerror(errno));
		return 0;
	}

	fseek(file, 0, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* fileBuffer = malloc(fileSize + 1);
	fread(fileBuffer, 1, fileSize, file);
	fclose(file);

	fp_frameid frameid = fp_ppm_create_frame(fileBuffer, fileSize);

	free(fileBuffer);

	return frameid;
}

/* fp_frameid fp_create_image_frame() { */
/* 	fp_frameid frameId = fp_frame_create(width, height, rgb(0,0,0)); */
/* } */
