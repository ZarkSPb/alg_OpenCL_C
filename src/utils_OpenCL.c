#include <stdio.h>
#include <stdlib.h>
#if defined(__APPLE__) || defined(__MACOSX)
    #include <OpenCL/opencl.h>
#else
    #include <CL/cl.h>
#endif

#include "utils_OpenCL.h"

#define PATHTOCL "src/resize.cl"

cl_context context;
cl_command_queue queue;
cl_program program;

cl_platform_id* get_opencl_platforms(cl_uint *num_platforms) {
    cl_int status;

    // Получение количества доступных платформ
    status = clGetPlatformIDs(0, NULL, num_platforms);
    if (status != CL_SUCCESS) {
        printf("Ошибка при получении количества платформ: %d\n", status);
        return NULL;
    }

    // Выделение памяти для хранения идентификаторов платформ
    cl_platform_id *platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * (*num_platforms));
    if (platforms == NULL) {
        printf("Ошибка выделения памяти для платформ.\n");
        return NULL;
    }

    // Получение списка платформ
    status = clGetPlatformIDs(*num_platforms, platforms, NULL);
    if (status != CL_SUCCESS) {
        printf("Ошибка при получении платформ: %d\n", status);
        free(platforms);
        return NULL;
    }

    return platforms;
}

cl_device_id get_default_device(cl_bool use_gpu) {
    cl_uint num_platforms;
    cl_platform_id* platforms = get_opencl_platforms(&num_platforms);

    if (platforms == NULL || num_platforms == 0) {
        printf("Не удалось получить платформы OpenCL.\n");
        return NULL;
    }

    cl_device_id best_device = NULL;
    cl_ulong max_global_mem_size = 0;

    for (cl_uint i = 0; i < num_platforms; i++) {
        cl_device_type device_type = use_gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU;
        cl_uint num_devices;
        cl_int status;

        // Получение количества устройств нужного типа
        status = clGetDeviceIDs(platforms[i], device_type, 0, NULL, &num_devices);
        if (status != CL_SUCCESS || num_devices == 0) {
            continue; // Платформа не поддерживает устройства нужного типа, пропускаем
        }

        // Получение устройств
        cl_device_id* devices = (cl_device_id*)malloc(sizeof(cl_device_id) * num_devices);
        status = clGetDeviceIDs(platforms[i], device_type, num_devices, devices, NULL);
        if (status != CL_SUCCESS) {
            free(devices);
            continue; // Пропускаем в случае ошибки
        }

        // Выбор устройства с наибольшим объемом глобальной памяти
        for (cl_uint j = 0; j < num_devices; j++) {
            cl_ulong global_mem_size;
            clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(global_mem_size), &global_mem_size, NULL);

            if (global_mem_size > max_global_mem_size) {
                max_global_mem_size = global_mem_size;
                best_device = devices[j];
            }
        }

        free(devices);
    }

    free(platforms);

    if (best_device != NULL) {
        char device_name[128];
        char platform_name[128];
        clGetDeviceInfo(best_device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
        cl_platform_id platform_id;
        clGetDeviceInfo(best_device, CL_DEVICE_PLATFORM, sizeof(platform_id), &platform_id, NULL);
        clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL);
        
        printf("\nИспользуем %s: %s\n", use_gpu ? "GPU" : "CPU", device_name);
        printf("На платформе: %s\n\n", platform_name);
    } else {
        printf("Не найдено подходящих устройств OpenCL.\n");
    }

    return best_device;
}

int get_context_queue_prog(cl_device_id device) {
    cl_int status;

    // Создание контекста
    context = clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    if (status != CL_SUCCESS) {
        printf("Ошибка при создании контекста: %d\n", status);
        return status;
    }

    // Создание командной очереди
    queue = clCreateCommandQueue(context, device, 0, &status);
    if (status != CL_SUCCESS) {
        printf("Ошибка при создании командной очереди: %d\n", status);
        clReleaseContext(context);
        return status;
    }

    // Чтение и компиляция OpenCL программы
    FILE *fp = fopen(PATHTOCL, "r");
    if (!fp) {
        printf("Ошибка при открытии файла: %s\n", PATHTOCL);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t source_size = ftell(fp);
    rewind(fp);
    
    char *source_str = (char *)malloc(source_size + 1);
    fread(source_str, 1, source_size, fp);
    fclose(fp);
    source_str[source_size] = '\0';

    program = clCreateProgramWithSource(context, 1, (const char **)&source_str, &source_size, &status);
    free(source_str);

    if (status != CL_SUCCESS) {
        printf("Ошибка при создании программы: %d\n", status);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        return status;
    }

    // Компиляция программы
    status = clBuildProgram(program, 1, &device, "-Werror", NULL, NULL);
    if (status != CL_SUCCESS) {
        // Вывод лога сборки в случае ошибки
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = (char *)malloc(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        printf("Build log:\n%s\n", log);
        free(log);

        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        return status;
    }

    // Если сборка прошла успешно, выводим лог
    size_t log_size;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    char *log = (char *)malloc(log_size);
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
    printf("Build log:\n%s\n", log);
    free(log);

    return CL_SUCCESS;
}