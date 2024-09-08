#ifndef UTILS_OPENCL
#define UTILS_OPENCL

extern cl_context context;
extern cl_command_queue queue;
extern cl_program program;
cl_device_id get_default_device(cl_bool use_gpu);
int get_context_queue_prog(cl_device_id device);

#endif // UTILS_OPENCL