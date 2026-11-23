/***************************************
 * @file jpegrw.c
 * @author Joseph Hernandez
 * @date 11/22/26
 * @assignment CPE2600-111 Lab 11 
 *
 ***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <jpeglib.h>

#include "jpegrw.h"

#define NUM_COMPONENTS 3

/*
 * Create a blank RGB image.
 *
 * Each pixel requires three components:
 * red, green, and blue.
 */
imgRawImage *initRawImage(unsigned int width, unsigned int height) {
    imgRawImage *newImg;
    size_t bufferSize;
    // check valid dimensions
    if (width == 0 || height == 0) {
        return NULL;
    }
    // check for overflow
    if ((size_t)width > SIZE_MAX / (size_t)height){
        return NULL;
    }

    bufferSize = (size_t)width * (size_t)height;
    if (bufferSize > SIZE_MAX / NUM_COMPONENTS){
        return NULL;
    }

    bufferSize *= NUM_COMPONENTS;
    newImg = malloc(sizeof(imgRawImage)); // malloc image size
    if (newImg == NULL){
        return NULL;
    }

    newImg->lpData = malloc(bufferSize); // malloc for pixel data
    if (newImg->lpData == NULL){
        free(newImg);
        return NULL;
    }
    newImg->numComponents = NUM_COMPONENTS;
    newImg->width = width;
    newImg->height = height;
    return newImg;
}

//free the image
void freeRawImage(imgRawImage *img) {
    if (img != NULL) {
        free(img->lpData);
        free(img);
    }
}

void setImageRGB(imgRawImage *image, unsigned char red, unsigned char green, unsigned char blue) {
    unsigned int x;
    unsigned int y;
    size_t index;

    if (image == NULL || image->lpData == NULL) {
        return;
    }

    for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width; x++) {
            index = ((size_t)y * image->width + x) * image->numComponents;
            image->lpData[index] = red;
            image->lpData[index + 1] = green;
            image->lpData[index + 2] = blue;
        }
    }
}

/*
 * Convert a hex into a color:
 * 0xFF0000 = red
 * 0x00FF00 = green
 * 0x0000FF = blue
 */

void setImageCOLOR(imgRawImage *image, unsigned int rgb) {
    setImageRGB(image, (unsigned char)((rgb & 0xFF0000) >> 16), (unsigned char)((rgb & 0x00FF00) >> 8), (unsigned char)(rgb & 0x0000FF));
}

void setPixelRGB(imgRawImage *image, unsigned int x, unsigned int y, unsigned char red, unsigned char green, unsigned char blue) {
    size_t index;
    if (image == NULL || image->lpData == NULL) {
        return;
    }
    // make sure coordinates are within the image
    if (x >= image->width || y >= image->height) {
        return;
    }
    y = image->height - y - 1;
    index = ((size_t)y * image->width + x) * image->numComponents;
    image->lpData[index] = red;
    image->lpData[index + 1] = green;
    image->lpData[index + 2] = blue;
}

void setPixelCOLOR(imgRawImage *image, unsigned int x, unsigned int y, unsigned int rgb) {
    setPixelRGB(image, x, y, (unsigned char)((rgb & 0xFF0000) >> 16), (unsigned char)((rgb & 0x00FF00) >> 8), (unsigned char)(rgb & 0x0000FF));
}

// load jpeg into image file and convert to RGB
imgRawImage *loadJpegImageFile(const char *lpFilename) {
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;

    imgRawImage *newImage = NULL;

    unsigned char *rowBuffer[1];

    FILE *fileHandle;
    fileHandle = fopen(lpFilename, "rb");
    if (fileHandle == NULL) {
        fprintf(stderr, "Unable to open JPEG file: %s\n", lpFilename);
        return NULL;
    }

    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);
    jpeg_stdio_src(&info, fileHandle);
    jpeg_read_header(&info, TRUE);

    info.out_color_space = JCS_RGB;
    jpeg_start_decompress(&info);

    // make sure it is in rgb (3 color components)
    if (info.output_components != NUM_COMPONENTS || info.output_width > UINT_MAX || info.output_height > UINT_MAX) {
        fprintf(stderr, "Unsupported JPEG image dimensions or format.\n");

        jpeg_destroy_decompress(&info);
        fclose(fileHandle);

        return NULL;
    }

    // make an image based on jpeg height and width
    newImage = initRawImage((unsigned int)info.output_width, (unsigned int)info.output_height);

    // make sure it exists
    if (newImage == NULL) {
        fprintf(stderr, "Unable to allocate JPEG image memory.\n");
        jpeg_destroy_decompress(&info);
        fclose(fileHandle);
        return NULL;
    }

    while (info.output_scanline < info.output_height) {
        size_t rowIndex;
        rowIndex = (size_t)info.output_scanline * newImage->width * NUM_COMPONENTS;
        rowBuffer[0] = &newImage->lpData[rowIndex];

        if (jpeg_read_scanlines(&info, rowBuffer, 1) != 1) {
            fprintf(stderr, "Unable to read JPEG scanline.\n");
            freeRawImage(newImage);
            jpeg_destroy_decompress(&info);
            fclose(fileHandle);
            return NULL;
        }
    }

    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
    fclose(fileHandle);
    return newImage;
}

// method for saving an rgb image as a jpeg file
int storeJpegImageFile(const imgRawImage *lpImage, const char *lpFilename) {
    struct jpeg_compress_struct info;
    struct jpeg_error_mgr err;
    unsigned char *rowBuffer[1];
    FILE *fileHandle;
    if (lpImage == NULL || lpImage->lpData == NULL || lpImage->width == 0 || lpImage->height == 0 || lpImage->numComponents != NUM_COMPONENTS) {
        fprintf(stderr, "Invalid image supplied to JPEG writer.\n");
        return 1;
    }

    fileHandle = fopen(lpFilename, "wb");
    if (fileHandle == NULL) {
        fprintf(stderr, "Unable to open output file: %s\n", lpFilename);
        return 1;
    }

    info.err = jpeg_std_error(&err);
    jpeg_create_compress(&info);
    jpeg_stdio_dest(&info, fileHandle);
    info.image_width = lpImage->width;
    info.image_height = lpImage->height;
    info.input_components = NUM_COMPONENTS;
    info.in_color_space = JCS_RGB;
    jpeg_set_defaults(&info);
  
    jpeg_set_quality(&info, 95, TRUE);
    jpeg_start_compress(&info, TRUE);

// write the image
    while (info.next_scanline < info.image_height) {
        size_t rowIndex;
        rowIndex = (size_t)info.next_scanline * lpImage->width * NUM_COMPONENTS;

        rowBuffer[0] = &lpImage->lpData[rowIndex];
        jpeg_write_scanlines(&info, rowBuffer, 1);
    }

    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    fclose(fileHandle);

    return 0;
}

