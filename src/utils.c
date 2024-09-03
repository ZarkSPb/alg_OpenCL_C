#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

// int create_directory(const char *base_path, const char *folder_name) {
int create_directory(const char *folder_name) {
    if (mkdir(folder_name, 0700) == -1) {
        if(errno == EEXIST){
            printf("\nFolder already exist. Go NEXT.\n");
            return 0;
        } else {
            perror("Error crearting directory");
            return -1;
        }
    }
    return 0; // Directory is successfully created
}

char **get_filenames(const char *directory, int *file_count) {
    int array_size = 10; // Начальный размер массива
    char **file_names = malloc(array_size * sizeof(char*));

    if (file_names == NULL) {
        perror("Unableto allocate memory");
        return NULL;
    }

    DIR *dir = opendir(directory);
    if (dir == NULL) {
        perror("Unable to open directory");
        return NULL;
    }

    // Чтение содержимого директории с .raw файлами
    struct dirent *entry;
    *file_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Учитываем только файлы
            if (*file_count >= array_size) {
                // Увеличение размера массива при необходимости
                array_size *= 2;
                char **new_file_paths = realloc(file_names, array_size * sizeof(char*));
                if (new_file_paths == NULL) {
                    perror("Unableto reallocate memory");
                    free(file_names);
                    closedir(dir);
                    return NULL;
                }
                file_names = new_file_paths;
            }

            // Формирование имени файла
            int filename_len = strlen(entry->d_name) + 1;
            file_names[*file_count] = malloc(filename_len);
            if (file_names[*file_count] == NULL) {
                perror("Unable to allocate memory for filename");
                break;
            }
            snprintf(file_names[*file_count], filename_len, "%s", entry->d_name);
            (*file_count)++;
        }
    }

    closedir(dir);
    return file_names;
}

int check_extension(const char *filename, const char *extension) {
    size_t filename_len = strlen(filename);
    size_t extension_len = strlen(extension);

    if (filename_len >= extension_len) {
        if (strcmp(filename + filename_len - extension_len, extension) == 0) {
            return 1; // Расширение совпадает
        }
    }
    return 0; // Расширение не совпадает
}

uint16_t *load_image_flatten(const char *filename, long length) {
    // Открываем файл для чтения
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Eror opening file");
        return NULL;
    }

    // Вычисляем количество элементов в файле
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    int num_elements = file_size / sizeof(uint16_t);

    // Выделяем память для данных
    uint16_t *img16_flat = malloc(file_size);
    if (img16_flat == NULL) {
        perror("Unable to allocate memory");
        fclose(file);
        return NULL;
    }

    // Чтение данных из файла
    fread(img16_flat, sizeof(uint16_t), num_elements, file);
    fclose(file);

    // Преобразование данных в изображение 16(12) бит на width x height
    if (num_elements != length) {
        fprintf(stderr, "Error: Incorrect file size for the expected dimensions.\n");
        free(img16_flat);
        return NULL;
    }

    return img16_flat;
}

char *get_filename_without_extension(const char *filename) {
    // Найдем последнюю точку в строке, которая отделяет расширение
    char *dot = strrchr(filename, '.');

    // Определим длину имени файла без расширения
    size_t length = (dot == NULL) ? strlen(filename) : (size_t)(dot - filename);

    // Выделим память для нового имени файла без расширения
    char *result = malloc(length + 1); // +1 для завершающего нуля
    if (result == NULL) {
        perror("Unable to allocate memory");
        return NULL;
    }

    // Скопируем имя файла без расширения в новый массив
    strncpy(result, filename, length);
    result[length] = '\0'; // Добавляем завершающий нуль

    return result;
}