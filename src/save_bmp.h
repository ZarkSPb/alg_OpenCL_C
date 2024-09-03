#ifndef SAVE_BMP_H
#define SAVE_BMP_H

#include <stdint.h>

void save_buffer_bmp(const char *filename, uint8_t *buffer, int width, int height);

#endif //SAVE_BMP_H