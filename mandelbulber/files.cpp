/********************************************************
 /                   MANDELBULBER                        *
 /                                                       *
 / author: Krzysztof Marczak                             *
 / contact: buddhi1980@gmail.com                         *
 / licence: GNU GPL                                      *
 ********************************************************/

/*
 * files.cpp
 *
 *  Created on: 2010-01-23
 *      Author: krzysztof
 */

#include <stdio.h>
#define PNG_DEBUG 3
#include <png.h>
//#include <jpeglib.h>
#include "files.h"
#include <setjmp.h>
#include <string.h>
#include "image.h"
#include "cimage.hpp"
#include "Render3D.h"

extern "C"
{
#include <jpeglib.h>
}

char logfileName[1000];

//***************************** Index filename *************************
//funkcja numerująca pliki
//we:	filename - nazwa pliku bez rozszerzenia
//		extension - rozszerzenie
//		number - numer do dodania
//wy:	fullname - nazwa pliku z numerem i rozszerzeniem
//		return - ilość znaków
int IndexFilename(char* fullname, char* filename, char* extension, int number)
{
	int strLength;
	strLength = sprintf(fullname, "%s%.5i.%s", filename, number, extension);
	return strLength;
}

METHODDEF(void) my_error_exit(j_common_ptr cinfo)
{
	/* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	my_error_ptr myerr = (my_error_ptr) cinfo->err;

	/* Always display the message. */
	/* We could postpone this until after returning, if we chose. */
	(*cinfo->err->output_message)(cinfo);

	/* Return control to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}

int LoadJPEG(char *filename, JSAMPLE *image)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	FILE * infile; /* source file */
	JSAMPARRAY buffer; /* Output row buffer */
	int row_stride; /* physical row width in output buffer */

	if ((infile = fopen(filename, "rb")) == NULL)
	{
		fprintf(stderr, "can't open %s\n", filename);
		return 0;
	}

	/* Step 1: allocate and initialize JPEG decompression object */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return 0;
	}
	jpeg_create_decompress(&cinfo);

	/* Step 2: specify data source (eg, a file) */
	jpeg_stdio_src(&cinfo, infile);

	/* Step 3: read file parameters with jpeg_read_header() */
	(void) jpeg_read_header(&cinfo, TRUE);

	/* Step 4: set parameters for decompression */

	/* Step 5: Start decompressor */
	(void) jpeg_start_decompress(&cinfo);

	/* JSAMPLEs per row in output buffer */
	row_stride = cinfo.output_width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with image */
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	/* Step 6: while (scan lines remain to be read) */
	/*           jpeg_read_scanlines(...); */
	while (cinfo.output_scanline < cinfo.output_height)
	{
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);
		//put_scanline_someplace(buffer[0], row_stride);
		memcpy(&image[(cinfo.output_scanline - 1) * row_stride], buffer[0], (int) row_stride);
	}
	/* Step 7: Finish decompression */
	(void) jpeg_finish_decompress(&cinfo);

	/* Step 8: Release JPEG decompression object */
	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
	return 1;
}

//************************** CheckJPEGsize *******************************
bool CheckJPEGsize(char *filename, int *width, int *height)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	FILE * infile; /* source file */

	if ((infile = fopen(filename, "rb")) == NULL)
	{
		fprintf(stderr, "can't open %s\n", filename);
		return false;
	}

	/* Step 1: allocate and initialize JPEG decompression object */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return 0;
	}
	jpeg_create_decompress(&cinfo);

	/* Step 2: specify data source (eg, a file) */
	jpeg_stdio_src(&cinfo, infile);

	/* Step 3: read file parameters with jpeg_read_header() */
	(void) jpeg_read_header(&cinfo, TRUE);

	*width = cinfo.image_width;
	*height = cinfo.image_height;

	/* Step 8: Release JPEG decompression object */
	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
	return true;
}

//************************** Save JPEG ***********************************
void SaveJPEG(char *filename, int quality, int width, int height, JSAMPLE *image)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE * outfile;
	JSAMPROW row_pointer[1];
	int row_stride;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	if ((outfile = fopen(filename, "wb")) == NULL)
	{
		fprintf(stderr, "can't open %s\n", filename);
		return;
	}
	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3; /* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB; /* colorspace of input image */

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	row_stride = width * 3; /* JSAMPLEs per row in image_buffer */

	while (cinfo.next_scanline < cinfo.image_height)
	{
		row_pointer[0] = &image[cinfo.next_scanline * row_stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);
}

void SavePNG(char *filename, int quality, int width, int height, png_byte *image)
{
	/* create file */
	FILE *fp = fopen(filename, "wb");
	if (!fp) fprintf(stderr, "[write_png_file] File %s could not be opened for writing", filename);

	/* initialize stuff */
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr) fprintf(stderr, "[write_png_file] png_create_write_struct failed");

	png_info *info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		fprintf(stderr, "[write_png_file] png_create_info_struct failed");
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during init_io");
		return;
	};

	png_init_io(png_ptr, fp);

	/* write header */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during writing header");
		return;
	}

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during writing bytes");
		return;
	}

	png_bytep *row_pointers = new png_bytep[height];
	for (int y = 0; y < height; y++)
	{
		row_pointers[y] = &image[y * width * 3];
	}

	png_write_image(png_ptr, row_pointers);

	/* end write */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during end of write");
		return;
	}

	png_write_end(png_ptr, NULL);

	delete[] row_pointers;

	fclose(fp);
}

void SavePNG16(char *filename, int quality, int width, int height, png_byte *image)
{
	/* create file */
	FILE *fp = fopen(filename, "wb");
	if (!fp) fprintf(stderr, "[write_png_file] File %s could not be opened for writing", filename);

	/* initialize stuff */
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr) fprintf(stderr, "[write_png_file] png_create_write_struct failed");

	png_info *info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		fprintf(stderr, "[write_png_file] png_create_info_struct failed");
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during init_io");
		return;
	};

	png_init_io(png_ptr, fp);

	/* write header */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during writing header");
		return;
	}

	png_set_IHDR(png_ptr, info_ptr, width, height, 16, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);
	png_set_swap(png_ptr);

	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during writing bytes");
		return;
	}

	png_bytep *row_pointers = new png_bytep[height];
	sRGB16 *image16 = (sRGB16*) mainImage->ConvertTo8bit();

	for (int y = 0; y < height; y++)
	{
		row_pointers[y] = (png_byte*) &image16[y * width];
	}

	png_write_image(png_ptr, row_pointers);

	/* end write */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during end of write");
		return;
	}

	png_write_end(png_ptr, NULL);

	delete[] row_pointers;

	fclose(fp);
}

void SavePNG16Alpha(char *filename, int quality, int width, int height, png_byte *image)
{
	/* create file */
	FILE *fp = fopen(filename, "wb");
	if (!fp) fprintf(stderr, "[write_png_file] File %s could not be opened for writing", filename);

	/* initialize stuff */
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr) fprintf(stderr, "[write_png_file] png_create_write_struct failed");

	png_info *info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		fprintf(stderr, "[write_png_file] png_create_info_struct failed");
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during init_io");
		return;
	};

	png_init_io(png_ptr, fp);

	/* write header */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during writing header");
		return;
	}

	png_set_IHDR(png_ptr, info_ptr, width, height, 16, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);
	png_set_swap(png_ptr);

	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during writing bytes");
		return;
	}

	png_bytep *row_pointers = new png_bytep[height];
	sImageRGBA16 *image16 = new sImageRGBA16[width * height];

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int ptr = x+y*width;
			sRGB16 pixel = mainImage->GetPixelImage(x,y);
			image16[ptr].R = pixel.R;
			image16[ptr].G = pixel.G;
			image16[ptr].B = pixel.B;
			image16[ptr].A = mainImage->GetPixelAlpha(x,y);
		}
	}

	for (int y = 0; y < height; y++)
	{
		row_pointers[y] = (png_byte*) &image16[y * width];
	}

	png_write_image(png_ptr, row_pointers);

	/* end write */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "[write_png_file] Error during end of write");
		return;
	}

	png_write_end(png_ptr, NULL);

	delete[] row_pointers;
	delete[] image16;

	fclose(fp);
}

bool FileIfExist(char *filename)
{
	FILE *file;
	file = fopen(filename, "r");
	if (file)
	{
		fclose(file);
		return true;
	}
	else return false;
}

void WriteLog(const char *text)
{
	FILE *logfile = fopen(logfileName, "a");
	fprintf(logfile, "%d: %s\n", (int) clock(), text);
	fclose(logfile);
}

void WriteLogDouble(const char *text, double value)
{
	FILE *logfile = fopen(logfileName, "a");
	fprintf(logfile, "%d: %s, value = %g\n", (int) clock(), text, value);
	fclose(logfile);
}
