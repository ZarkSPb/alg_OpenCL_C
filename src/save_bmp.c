#include "save_bmp.h"
#include <stdio.h>
#include <stdint.h>

// Struct for BMP header
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      // Тип файла ("BM")
    uint32_t bfSize;      // Размер файла в байтах
    uint16_t bfReserved1; // Зарезервировано
    uint16_t bfReserved2; // Зарезервировано
    uint32_t bfOffBits;   // Смещение до начала данных изображения
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;          // Размер структуры BITMAPINFOHEADER
    int32_t  biWidth;         // Ширина изображения
    int32_t  biHeight;        // Высота изображения
    uint16_t biPlanes;        // Количество цветовых плоскостей (должно быть 1)
    uint16_t biBitCount;      // Количество бит на пиксель (1, 4, 8, 16, 24 или 32)
    uint32_t biCompression;   // Тип сжатия (0 - без сжатия)
    uint32_t biSizeImage;     // Размер изображения в байтах (может быть 0 для несжатых изображений)
    int32_t  biXPelsPerMeter; // Горизонтальное разрешение (пикселей на метр)
    int32_t  biYPelsPerMeter; // Вертикальное разрешение (пикселей на метр)
    uint32_t biClrUsed;       // Количество используемых цветов в палитре
    uint32_t biClrImportant;  // Количество важнейших цветов
} BITMAPINFOHEADER;

#pragma pack(pop)

// TODO оптимизировать эту функцию - вынести заполнение структур и выяснения размеров
// вынести заполнение палитры
void save_buffer_bmp(const char *filename, uint8_t *buffer, int width, int height) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Unable to open file for writing");
        return;
    }

    int row_stride = (width + 3) & ~3; // Учитываем выравнивание до 4 байт
    int image_size = row_stride * height;

    // Заполняем заголовок файла BMP
    BITMAPFILEHEADER file_header;
    file_header.bfType = 0x4D42; // BM
    file_header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + image_size;
    file_header.bfReserved1 = 0;
    file_header.bfReserved2 = 0;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // Заполняем заголовок информации о BMP
    BITMAPINFOHEADER info_header;
    info_header.biSize = sizeof(BITMAPINFOHEADER);
    info_header.biWidth = width;
    info_header.biHeight = height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 8; // 8 бит на пиксель
    info_header.biCompression = 0; // BI_RGB, без сжатия
    info_header.biSizeImage = image_size;
    info_header.biXPelsPerMeter = 0;
    info_header.biYPelsPerMeter = 0;
    info_header.biClrUsed = 256; // 256 цветов в палитре
    info_header.biClrImportant = 256; // Все цвета важны

    // Запись заголовков в файл
    fwrite(&file_header, sizeof(BITMAPFILEHEADER), 1, file);
    fwrite(&info_header, sizeof(BITMAPINFOHEADER), 1, file);

    // Запись палитры (грейскейл от 0 до 255)
    for (int i = 0; i < 256; i++) {
        uint8_t color[4] = {i, i, i, 0}; // RGB и нулевой байт
        fwrite(color, sizeof(color), 1, file);
    }

    // Запись пикселей изображения с учетом выравнивания строк
    uint8_t padding[3] = {0, 0, 0}; // Максимум 3 байта padding
    for (int y = height - 1; y >= 0; y--) {
        fwrite(buffer + y * width, 1, width, file); // Запись строки изображения
        fwrite(padding, 1, row_stride - width, file); // Запись padding
    }

    fclose(file);
    // printf("\nBMP file saved successfully as %s\n\n", filename);
}