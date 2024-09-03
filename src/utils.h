#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

int create_directory(const char *folder_name);
char **get_filenames(const char *directory, int *file_count);
int check_extension(const char *filename, const char *extension);
uint16_t *load_image_flatten(const char *filename, long length);
char *get_filename_without_extension(const char *filename);

#endif // UTILS_H