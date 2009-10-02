#include "i.h"

// popen/pclose is not c99
extern FILE *popen(const char *command, const char *type); 
extern int pclose(FILE *stream);

FILE *xv_open(int width, int height) {
	FILE *f = popen("xv -", "w");
	if (f == NULL)
		die("Can not popen xv");
	return f;
}

void xv_close(FILE *f) {
//	dp(0,"\n");
//	fclose(f);
//	dp(0,"\n");
}

void xv_double_array(int dimension1, int dimension2, double array[dimension1][dimension2], double max, int transpose, int invert) {
	FILE *f = xv_open(dimension1, dimension2);
	write_array_as_image(f, dimension1, dimension2, array, max, transpose, invert);
	xv_close(f);
}

void write_array_as_image(FILE *f, int dimension1, int dimension2, double array[dimension1][dimension2], double max, int transpose, int invert) {
	dp(21, "write_array_as_image(%d, %d, %p, %f, %d, %d)\n", dimension1, dimension2, array, max, transpose, invert);
	const int max_pixel = 65535;
	double min = 0;
	if (!max) {
		min = array[0][0];
		max = array[0][0];
		for (int row = 0; row < dimension1; row++)
			for (int column = 0; column < dimension2; column++) {
				double x = array[row][column];
				if (x > max)
					max = x;
				else if (x < min)
					min = x;
			}
	}
	double range =  max - min;
	dp(21, "max=%g\n", max);			
	if (transpose) {
		fprintf(f, "P5 %d %d %d\n", dimension2, dimension1, max_pixel);
		for (int row = 0; row < dimension1; row++) {
			int r = row;
			if (invert)
				r = dimension1 - 1 - row;
			uint16_t line[dimension2];
			for (int column = 0; column < dimension2; column++) {
				int pixel = 0.5+max_pixel*(array[r][column]-min)/range;
				if (pixel < 0)
					pixel = 0;
				else if (pixel > max_pixel)
					pixel = max_pixel;
	//			printf("%d\n", pixel);
				line[column] = pixel; // machine must be big-endian
			}
			fwrite(line, sizeof line, 1, f);
		}
	} else {
		fprintf(f, "P5 %d %d %d\n", dimension1, dimension2, max_pixel);
		for (int row = 0; row < dimension2; row++) {
			int r = row;
			if (invert)
				r = dimension2 - 1 - row;
			uint16_t line[dimension1];
			for (int column = 0; column < dimension1; column++) {
				int pixel = 0.5+max_pixel*(array[column][r]-min)/range;
				if (pixel < 0)
					pixel = 0;
				else if (pixel > max_pixel)
					pixel = max_pixel;
	//			printf("%d\n", pixel);
				line[column] = pixel; // machine must be big-endian
			}
			fwrite(line, sizeof line, 1, f);
		}
	}
}

void write_arrays_as_rgb_ppm(char *filename, int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert) {
	dp(21, "write_arrays_as_rgb_ppm(%d, %d, %d, %d)\n", dimension1, dimension2, transpose, invert);
	FILE *f = fopen(filename, "w");
	assert(f);
	write_arrays_as_rgb_image(f, dimension1, dimension2, red, green, blue, transpose, invert);
	fclose(f);
}

void write_arrays_as_rgb_jpg(char *filename, int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert, char *resize) {
	dp(21, "write_arrays_as_rgb_jpg(%d, %d, %d, %d)\n", dimension1, dimension2, transpose, invert);
	FILE *f;
//	sprintf(buffer, "pnmtojpeg '--quality=100' -  >%s", filename);
	gchar *command = resize ?  g_strdup_printf("convert -  -quality 100 -resize '%s' '%s'", resize, filename) : g_strdup_printf("convert -  -quality 100 '%s'", filename);
	dp(22, "command='%s'\n", command);
	f = popen(command, "w");
	assert(f);
	write_arrays_as_rgb_image(f, dimension1, dimension2, red, green, blue, transpose, invert);
	fclose(f);
	g_free(command);
}

void write_arrays_as_rgb_png(char *filename, int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert, char *resize) {
	dp(21, "write_arrays_as_rgb_png(%d, %d, %d, %d)\n", dimension1, dimension2, transpose, invert);
	FILE *f = fopen(filename, "w");
	assert(f);
	write_arrays_as_png_image(f, dimension1, dimension2, red, green, blue, transpose, invert);
	fclose(f);
}

void xv_double_array_colour(int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert) {
	FILE *f = xv_open(dimension1, dimension2);
	write_arrays_as_rgb_image(f, dimension1, dimension2, red, green, blue, transpose, invert);
	xv_close(f);
}

void write_arrays_as_rgb_image(FILE *f, int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert) {
	dp(21, "write_arrays_as_rgb_image(%d, %d, transpose=%d, invert=%d)\n", dimension1, dimension2, transpose, invert);
	const int max_pixel = 255;
	double min_value[3], max_value[3];
	void *arrays[3];
	arrays[0] = red;
	arrays[1] = green;
	arrays[2] = blue;
	for (int colour = 0; colour < 3; colour++) {
		double (*array)[dimension2] = arrays[colour];
		if (!array) {
			min_value[colour] = 0;
			max_value[colour] = 0;
			continue;
		}
		double min = array[0][0];
		double max = array[0][0];
		for (int row = 0; row < dimension1; row++)
			for (int column = 0; column < dimension2; column++) {
				double x = array[row][column];
				if (x > max)
					max = x;
				else if (x < min)
					min = x;
			}
		min_value[colour] = min;
		max_value[colour] = max;
	}
	for (int colour = 0; colour < 3; colour++)
		dp(21, "max_value[%d]=%g min_value[%d]=%g\n", colour, max_value[colour], colour, min_value[colour]);			
	if (transpose) {
		fprintf(f, "P6 %d %d %d\n", dimension2, dimension1, max_pixel);
		for (int row = 0; row < dimension1; row++) {
			int r = row;
			if (invert)
				r = dimension1 - 1 - row;
			dp(31, "r=%d row=%d invert=%d\n", r, row, invert);
			uint8_t line[dimension2*3];
			for (int colour = 0; colour < 3; colour++) {
				double (*array)[dimension2] = arrays[colour];
				if (!array) {
					for (int column = 0; column < dimension2; column++)
						line[column*3+colour] = 0;
					continue;
				}
				double min = min_value[colour];
				double range = max_value[colour]-min;
				for (int column = 0; column < dimension2; column++) {
					int32_t pixel = 0.5+max_pixel*(array[r][column]-min)/range;
					if (pixel < 0)
						pixel = 0;
					else if (pixel > max_pixel)
						pixel = max_pixel;
					line[column*3+colour] = pixel;
				}
			}
			fwrite(line, sizeof line, 1, f);
		}
	} else {
		fprintf(f, "P6 %d %d %d\n", dimension1, dimension2, max_pixel);
		for (int row = 0; row < dimension2; row++) {
			int r = row;
			if (invert)
				r = dimension2 - 1 - row;
			uint8_t line[dimension1*3];
			for (int colour = 0; colour < 3; colour++) {
				double (*array)[dimension2] = arrays[colour];
				if (!array) {
					for (int column = 0; column < dimension1; column++)
						line[column*3+colour] = 0;
					continue;
				}
				double min = min_value[colour];
				double range = max_value[colour]-min;
				for (int column = 0; column < dimension1; column++) {
					int pixel = 0.5+max_pixel*(array[column][r]-min)/range;
					if (pixel < 0)
						pixel = 0;
					else if (pixel > max_pixel)
						pixel = max_pixel;
		//			printf("%d\n", pixel);
					line[column*3+colour] = pixel; // machine must be big-endian
				}
			}
			fwrite(line, sizeof line, 1, f);
		}
	}
}

#define PNG_DEBUG 3
#include <png.h>

void write_arrays_as_png_image(FILE *f, int dimension1, int dimension2, double red[dimension1][dimension2], double green[dimension1][dimension2], double blue[dimension1][dimension2], int transpose, int invert) {
	dp(21, "write_arrays_as_png(%d, %d, transpose=%d, invert=%d)\n", dimension1, dimension2, transpose, invert);
	int height = transpose ? dimension1 : dimension2;
	int width = transpose ? dimension2 : dimension1;
	png_bytep *row_pointers =  malloc(sizeof(png_bytep) * height);
	for (int y = 0; y < height; y++)
		row_pointers[y] = salloc(4 * width);
	const int max_pixel = 255;
	double min_value[3], max_value[3];
	void *arrays[3];
	arrays[0] = red;
	arrays[1] = green;
	arrays[2] = blue;
	for (int colour = 0; colour < 3; colour++) {
		double (*array)[dimension2] = arrays[colour];
		if (!array) {
			min_value[colour] = 0;
			max_value[colour] = 0;
			continue;
		}
		double min = array[0][0];
		double max = array[0][0];
		for (int row = 0; row < dimension1; row++)
			for (int column = 0; column < dimension2; column++) {
				double x = array[row][column];
				if (x > max)
					max = x;
				else if (x < min)
					min = x;
			}
		min_value[colour] = min;
		max_value[colour] = max;
	}
	for (int colour = 0; colour < 3; colour++)
		dp(21, "max_value[%d]=%g min_value[%d]=%g\n", colour, max_value[colour], colour, min_value[colour]);			
	if (transpose) {
		for (int row = 0; row < dimension1; row++) {
			int r = row;
			if (invert)
				r = dimension1 - 1 - row;
			dp(31, "r=%d row=%d invert=%d\n", r, row, invert);
			for (int colour = 0; colour < 3; colour++) {
				double (*array)[dimension2] = arrays[colour];
				if (!array) {
					for (int column = 0; column < dimension2; column++)
						row_pointers[row][column*3+colour] = 0;
					continue;
				}
				double min = min_value[colour];
				double range = max_value[colour]-min;
				for (int column = 0; column < dimension2; column++) {
					int32_t pixel = 0.5+max_pixel*(array[r][column]-min)/range;
					if (pixel < 0)
						pixel = 0;
					else if (pixel > max_pixel)
						pixel = max_pixel;
					row_pointers[row][column*4+colour] = pixel;
				}
			}
			for (int column = 0; column < dimension2; column++)
				row_pointers[row][column*4+3] = 255;
		}
	} else {
		for (int row = 0; row < dimension2; row++) {
			int r = row;
			if (invert)
				r = dimension2 - 1 - row;
			for (int colour = 0; colour < 3; colour++) {
				double (*array)[dimension2] = arrays[colour];
				if (!array) {
					for (int column = 0; column < dimension1; column++)
						row_pointers[row][column*3+colour] = 0;
					continue;
				}
				double min = min_value[colour];
				double range = max_value[colour]-min;
				for (int column = 0; column < dimension1; column++) {
					int pixel = 0.5+max_pixel*(array[column][r]-min)/range;
					if (pixel < 0)
						pixel = 0;
					else if (pixel > max_pixel)
						pixel = max_pixel;
		//			printf("%d\n", pixel);
					row_pointers[row][column*4+colour] = pixel; // machine must be big-endian
				}
			}
			for (int column = 0; column < dimension1; column++)
				row_pointers[row][column*4+3] = 255;
		}
	}
	write_png_file(f, width, height, row_pointers);
}

void write_png_file(FILE *fp, int width, int height, void *row_pointers ) {
	png_structp png_ptr;
	png_infop info_ptr;
	int color_type = PNG_COLOR_TYPE_RGBA;
	int bit_depth = 8;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		die("[write_png_file] png_create_write_struct failed");

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		die("[write_png_file] png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr)))
		die("[write_png_file] Error during init_io");

	png_init_io(png_ptr, fp);
	png_set_compression_level(png_ptr, param_get_integer_with_default("util", "png_compression_level", 1));

	if (setjmp(png_jmpbuf(png_ptr)))
		die("[write_png_file] Error during writing header");

	png_set_IHDR(png_ptr, info_ptr, width, height,
		     bit_depth, color_type, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	if (setjmp(png_jmpbuf(png_ptr)))
		die("[write_png_file] Error during writing bytes");
	png_write_image(png_ptr, row_pointers);

	if (setjmp(png_jmpbuf(png_ptr)))
		die("[write_png_file] Error during end of write");
	png_write_end(png_ptr, NULL);
}
