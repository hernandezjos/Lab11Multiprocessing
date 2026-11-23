/***************************************
 * @file jpegrw.h
 * @author Joseph Hernandez
 * @date 11/22/26
 * @assignment CPE2600-111 Lab 11 
 *
 ***************************************/

#ifndef JPEGRW_H
#define JPEGRW_H

typedef struct imgRawImage {
    unsigned int numComponents;
    unsigned int width;
    unsigned int height;
    unsigned char *lpData;
} imgRawImage; // holds raw image

imgRawImage *loadJpegImageFile(const char *fname); // load to mem
int storeJpegImageFile(const imgRawImage *img, const char *lpFilename); // save image as jpeg
imgRawImage *initRawImage(unsigned int width, unsigned int height); // makes blank image

void freeRawImage(imgRawImage *img);
void setImageCOLOR(imgRawImage *image, unsigned int rgb); // set pixels to a color
void setImageRGB(imgRawImage *image, unsigned char red, unsigned char green, unsigned char blue);
void setPixelRGB(imgRawImage *image, unsigned int x, unsigned int y, unsigned char red, unsigned char green, unsigned char blue);
void setPixelCOLOR(imgRawImage *image, unsigned int x, unsigned int y, unsigned int rgb);

#endif
