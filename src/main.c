#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <OpenCL/opencl.h>
#include "utils.h"
#include "save_bmp.h"
#include "utils_OpenCL.h"

void LUT(unsigned int *histogram);

#define WIDTH 2448
#define HEIGHT 2048
#define LENGTH WIDTH * HEIGHT
#define BINS 4096
#define BORDERSIZE 8

static long lut[4096];

int main() {
    const char *PROCFOLDER = "img/proc/";
    const char *RAWPATH = "../../Python/opencl/img/raw";
    const char *EXTENSION = ".raw";

    struct timeval start_all, end_all, start, end;
    long seconds, microseconds;
    double elapsed, time_clear = 0.0;

    cl_int status;
    cl_mem small_image_buff, result_image_buff, src_buff, histogram_buff, lut_buff;
    cl_kernel resize_kernel, pad_image_reflect_kernel, local_norm_kernel;

    // OpenCL initialise
    cl_device_id device = get_default_device(CL_TRUE);
    if (device == NULL) {
        printf("Не удалось выбрать устройство OpenCL.\n");
        return 1;
    }
    status = get_context_queue_prog(device);
    if (status != CL_SUCCESS) {
        printf("Не удалось создать контекст, очередь или программу.\n");
        return 1;
    }

    size_t global_size[2] = {WIDTH / 8, HEIGHT / 8};
    size_t pad_worksize[2] = {
        global_size[0] + 2 * BORDERSIZE,
        global_size[1] + 2 * BORDERSIZE,
    };

    // Создание OpenCL буферов
    small_image_buff = clCreateBuffer(context, CL_MEM_READ_WRITE,
        pad_worksize[0] * pad_worksize[1] * sizeof(unsigned char), NULL, &status);
    result_image_buff = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
        WIDTH * HEIGHT * sizeof(unsigned char), NULL, &status);

      // Получение ядра OpenCL для малого изображения и гистограммы
    resize_kernel = clCreateKernel(program, "resize", &status);
    if (status != CL_SUCCESS) {
        printf("Ошибка при создании ядра resize: %d\n", status);
        return 1;
    }
    // Получаем ядро для дополнения изображения
    pad_image_reflect_kernel = clCreateKernel(program, "pad_image_reflect", &status);
    if (status != CL_SUCCESS) {
        printf("Ошибка при создании ядра pad_image_reflect: %d\n", status);
        // clReleaseMemObject(lut_buff);
        return status;
    }
    // Получаем ядро для локальной нормализации (localNorm)
    local_norm_kernel = clCreateKernel(program, "localNorm", &status);
    if (status != CL_SUCCESS) {
        printf("Ошибка при создании ядра localNorm: %d\n", status);
        return status;
    }

    // Установка аргументов ядра
    clSetKernelArg(resize_kernel, 1, sizeof(cl_mem), &small_image_buff);

    // Создаем директорию для обработанных файлов
    if (create_directory(PROCFOLDER) != 0){
        return 1;
    }
    printf("Directory created successfully of already exists.\n");
    
    // Получаем имена файлов
    int file_count = 0;
    char **file_names = get_filenames(RAWPATH, &file_count);
    if (file_names == NULL) {
        fprintf(stderr, "Failed to retrieve file paths\n");
        return 1;
    }
    printf("Number of files: %d\n", file_count);

    // Выделяем память под результирующее изображение
    uint8_t *result = malloc(LENGTH);
    if (result == NULL) {
        perror("Unable to allocate memory for result");
        return 1;
    }
    
    char *filename_without_ext;
    int path_len;
    char path[1024];
    char *fn;
    unsigned short lut_short[BINS];

    uint c = 0;
    gettimeofday(&start_all, NULL);
    for (int i = 0; i < file_count; i++) {
        fn = file_names[i];
        // printf("%d ... %s\n", i, fn);
        // Проверяем расширение файла
        if (!check_extension(fn, EXTENSION)) { continue; }
        c++;
        filename_without_ext = get_filename_without_extension(fn);

        // Получам путь к файлу RAW
        path_len = strlen(RAWPATH) + strlen(fn) + 2;
        snprintf(path, path_len, "%s/%s", RAWPATH, fn);

        // Иницализаия переменной для хранения исходного плоского изображения
        uint16_t *img16_flat = NULL;
        // Загружаем изображение
        img16_flat = load_image_flatten(path, LENGTH);

        // NEXT - PROCESSING...
        gettimeofday(&start, NULL);

        // Small image resize and histogram reading
        // Подготавливаем буффер с исходным изображением
        src_buff = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            WIDTH * HEIGHT * sizeof(unsigned short), img16_flat, &status);
        // Создаем массив для гистограммы и его буффер
        unsigned int histogram[BINS] = {0}; // TODO надо каждый раз обнулять
        histogram_buff = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            sizeof(histogram), histogram, &status);

        // Устанавливаем аргументы (буфферы) ядра
        clSetKernelArg(resize_kernel, 0, sizeof(cl_mem), &src_buff);
        clSetKernelArg(resize_kernel, 2, sizeof(cl_mem), &histogram_buff);

        // !!!---Выполнение ядра---!!!
        status = clEnqueueNDRangeKernel(queue, resize_kernel, 2, NULL, global_size, NULL, 0, NULL, NULL);
        if (status != CL_SUCCESS) {
            printf("Ошибка при выполнении ядра: %d\n", status);
            return 1;
        }

        // Копирование данных обратно на хост
        status = clEnqueueReadBuffer(queue, histogram_buff, CL_TRUE, 0, sizeof(histogram), histogram, 0, NULL, NULL);
        if (status != CL_SUCCESS) {
            printf("Ошибка при чтении буфера гистограммы: %d\n", status);
            return 1;
        }

        clReleaseMemObject(histogram_buff);

        LUT(histogram);
        // Преобразуем каждый элемент в новый тип (unsigned short)
        for (int li = 0; li < BINS; li++) {
            lut_short[li] = (unsigned short)lut[li];
        }
        lut_buff = clCreateBuffer(context,
                                     CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                     sizeof(lut_short), lut_short, &status);
        if (status != CL_SUCCESS) {
            printf("Ошибка при создании LUT буфера: %d\n", status);
            clReleaseMemObject(lut_buff);
            return status;
        }


        // Padding
        // Установка аргументов ядра
        status = clSetKernelArg(pad_image_reflect_kernel, 0, sizeof(cl_mem), &small_image_buff);
        if (status != CL_SUCCESS) {
            printf("Ошибка при установке аргумента ядра: %d\n", status);
            clReleaseKernel(pad_image_reflect_kernel);
            clReleaseMemObject(lut_buff);
            return status;
        }

        // !!!---Запуск ядра---!!!
        status = clEnqueueNDRangeKernel(queue, pad_image_reflect_kernel, 2, NULL, pad_worksize, NULL, 0, NULL, NULL);
        if (status != CL_SUCCESS) {
            printf("Ошибка при выполнении ядра: %d\n", status);
            clReleaseKernel(pad_image_reflect_kernel);
            clReleaseMemObject(lut_buff);
            return status;
        }


        // Result image
        // Установка аргументов ядра
        status = clSetKernelArg(local_norm_kernel, 0, sizeof(cl_mem), &src_buff);
        status |= clSetKernelArg(local_norm_kernel, 1, sizeof(cl_mem), &small_image_buff);
        status |= clSetKernelArg(local_norm_kernel, 2, sizeof(cl_mem), &result_image_buff);
        status |= clSetKernelArg(local_norm_kernel, 3, sizeof(cl_mem), &lut_buff);
        if (status != CL_SUCCESS) {
            printf("Ошибка при установке аргументов ядра localNorm: %d\n", status);
            clReleaseKernel(local_norm_kernel);
            return status;
        }

        // Запуск ядра
        status = clEnqueueNDRangeKernel(queue, local_norm_kernel, 2, NULL, global_size, NULL, 0, NULL, NULL);
        if (status != CL_SUCCESS) {
            printf("Ошибка при выполнении ядра localNorm: %d\n", status);
            clReleaseKernel(local_norm_kernel);
            return status;
        }

        // Освобождение буферов
        clReleaseMemObject(src_buff);
        clReleaseMemObject(lut_buff);

        // Копирование результата обратно в host-память
        status = clEnqueueReadBuffer(queue, result_image_buff, CL_TRUE, 0, WIDTH * HEIGHT * sizeof(unsigned char), result, 0, NULL, NULL);
        if (status != CL_SUCCESS) {
            printf("Ошибка при копировании результата на хост: %d\n", status);
            clReleaseKernel(local_norm_kernel);
            free(result);
            return status;
        }

        gettimeofday(&end, NULL);
        seconds = end.tv_sec - start.tv_sec;
        microseconds = end.tv_usec - start.tv_usec;
        elapsed = seconds + microseconds*1e-6;
        time_clear += elapsed;


        // Формируем полный путь и записываем BMP
        path_len = strlen(PROCFOLDER) + 1 + strlen(filename_without_ext) + 8;
        snprintf(path, path_len, "%s%s_loc.bmp", PROCFOLDER, filename_without_ext);
        save_buffer_bmp(path, result, WIDTH, HEIGHT);

        // Освобождаем память загруженного изображения
        free(img16_flat);
        
        
        printf("%d: %f %s\n", c, elapsed, fn);
        // printf("%d: %s\n", i, fn);
    }
    gettimeofday(&end_all, NULL);
    seconds = end_all.tv_sec - start_all.tv_sec;
    microseconds = end_all.tv_usec - start_all.tv_usec;
    elapsed = seconds + microseconds*1e-6;

    printf("Time = %fs ... fps = %f\n", elapsed, file_count / elapsed);
    printf("Time clear = %fs ... fps clear = %f\n", time_clear, file_count / time_clear);

    // Освобождение памяти в file_names
    for (int i = 0; i < file_count; i++) {
        free(file_names[i]);
    }
    // Освобождаем память список имен файлов
    free(file_names);
    // Освобождаем память результирующего изображения
    free(result);

    // Очистка ресурсовclReleaseMemObject(small_image_buff);
    clReleaseMemObject(small_image_buff);
    clReleaseMemObject(result_image_buff);
    clReleaseKernel(resize_kernel);
    clReleaseKernel(pad_image_reflect_kernel);
    clReleaseKernel(local_norm_kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return 0;
}

void LUT(unsigned int *histogram) {
    // Вычисляем кумулятивную гистограмму входного изображения
    lut[0] = histogram[0];
    for (int i = 1; i < BINS; i++) {
        lut[i] = lut[i - 1] + histogram[i];
    }

    // Вычисляем таблицу преобразования
    int coefficient = WIDTH * HEIGHT / BINS;
    int i = 513;
    for (; i < BINS; i++) {
        lut[i] = lut[i] + coefficient * (i - 512);
    }
    // Ищем минимум и максимум
    long min = 0;
    for (int i = 0; i < BINS; i++) {
        if (lut[i] > 0) {
            min = lut[i];
            break;
        }
    }
    long diff = lut[BINS - 1] - min; // length * (1 + positionend / bin) - min
    for (unsigned short i = 0; i < BINS; i++) {
        // TODO тут тоже можно оптимизировать,
        // но общей производительности не прибавит
        lut[i] = 511 * (lut[i] - min) / diff;
        if (lut[i] < 0) {
            lut[i] = 0;
        }
    }
}