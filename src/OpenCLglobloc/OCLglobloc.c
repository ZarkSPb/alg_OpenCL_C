#include <stdio.h>
#include <sys/time.h>

#if defined(__APPLE__) || defined(__MACOSX)
    #include <OpenCL/opencl.h>
#else
    #include <CL/cl.h>
#endif

#include "utils_OpenCL.h"
#include "OCLglobloc.h"

#define BORDERSIZE 8

static long long lut[4096];
static unsigned short lut_short[4096];

static unsigned short WIDTH, HEIGHT, BINS = 4096;

cl_int status;
cl_mem small_image_buff, result_image_buff, src_buff, histogram_buff, lut_buff;
cl_kernel resize_kernel, pad_image_reflect_kernel, local_norm_kernel;

size_t global_size[2];
size_t pad_worksize[2];

void LUT(unsigned int *histogram);

int OCLproc_initialize(unsigned short width, unsigned short height, unsigned short bins) {
    WIDTH = width;
    HEIGHT = height;
    if (bins > 0) { BINS = bins; }

    // OpenCL initialize
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

    // Определяем размеры задач для ядер OpenCL
    global_size[0] = WIDTH / 8;
    global_size[1] = HEIGHT / 8;
    pad_worksize[0] = global_size[0] + 2 * BORDERSIZE;
    pad_worksize[1] = global_size[1] + 2 * BORDERSIZE;

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

    return 0;
}

void OCLproc_cleanup() {
    clReleaseMemObject(small_image_buff);
    clReleaseMemObject(result_image_buff);
    clReleaseKernel(resize_kernel);
    clReleaseKernel(pad_image_reflect_kernel);
    clReleaseKernel(local_norm_kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}

double OCLproc(uint16_t *image_flat, uint8_t *result) {
    struct timeval start, end;
    gettimeofday(&start, NULL);

    // Small image resize and histogram reading
    // Подготавливаем буффер с исходным изображением
    src_buff = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        WIDTH * HEIGHT * sizeof(unsigned short), image_flat, &status);
    // Создаем массив для гистограммы и его буффер
    unsigned int histogram[4096] = {0}; // TODO надо каждый раз обнулять
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
    lut_buff = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
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
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * 1e-6;
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