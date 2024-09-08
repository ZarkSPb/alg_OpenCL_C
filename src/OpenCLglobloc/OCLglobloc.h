#ifndef OCLGLOBLOC_H
#define OCLGLOBLOC_H

#include <stdint.h>

double OCLproc(uint16_t *image_flat, uint8_t *result);
int OCLproc_initialize(unsigned short width, unsigned short height, unsigned short bins);
void OCLproc_cleanup();

#endif // OCLGLOBLOC_H