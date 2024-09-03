#ifndef UTILS_OPENCL
#define UTILS_OPENCL

cl_context context;
cl_command_queue queue;
cl_program program;
cl_device_id get_default_device(cl_bool use_gpu);
int get_context_queue_prog(cl_device_id device);

#endif // UTILS_OPENCL