#define __CL_ENABLE_EXCEPTIONS
#define CL_SILENCE_DEPRECATION
#ifdef __APPLE_CC__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.hpp>
#endif

#include "gcv.h"

static cl_mem cli_init_buffer(cl_context ctx, cl_command_queue cq, void *buf,
                              size_t len) {
  int err = 0;

  cl_mem mem = clCreateBuffer(ctx, CL_MEM_READ_WRITE, len, NULL, NULL);
  if (buf)
    err = clEnqueueWriteBuffer(cq, mem, CL_TRUE, 0, len, buf, 0, NULL, NULL);
  return err ? NULL : mem;
}

#define COUNT 2048
#define BUF_SIZE 8192

int run_fibers_cl(uint32_t *salt, uint8_t *cipher, uint8_t *base, uint8_t *pass,
                  uint8_t *out, int count) {
  int err, fd, k_count = COUNT, cu_n;
  char name[64], log[BUF_SIZE];
  size_t len;

  char *src = malloc(BUF_SIZE);
  fd = open("./fiber.c", O_RDONLY);
  len = read(fd, src, BUF_SIZE);
  src[len] = 0;
  close(fd);

  // prepare device
  cl_device_id device_id;
  clGetDeviceIDs(NULL, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
  clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, 4, &cu_n, NULL);
  clGetDeviceInfo(device_id, CL_DEVICE_NAME, 64, name, NULL);

  // printf("using %s\n", name);
  // printf("  %d CUs\n", cu_n);

  // prepare kernel
  cl_context ctx = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
  // printf("%s %d\n", __func__, __LINE__);

  cl_command_queue cq = clCreateCommandQueue(ctx, device_id, 0, &err);
  // printf("%s %d\n", __func__, __LINE__);

  // printf("source is %s\n%zu\n", src, len);
  cl_program program =
      clCreateProgramWithSource(ctx, 1, (const char **)&src, NULL, &err);
  // printf("%s %d\n", __func__, __LINE__);
  err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  // printf("%s %d\n", __func__, __LINE__);

  if (err) {
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, BUF_SIZE,
                          log, NULL);
    printf("  build status:\n");
    printf("%s\n", log);
    return -1;
  }

  cl_kernel kernel = clCreateKernel(program, "fiber", &err);

  // prepare arguments
  cl_mem k_salt = cli_init_buffer(ctx, cq, salt, sizeof(uint32_t) * 256);
  cl_mem k_cipher = cli_init_buffer(ctx, cq, cipher, 1048);
  cl_mem k_base = cli_init_buffer(ctx, cq, base, 512);
  cl_mem k_pass = cli_init_buffer(ctx, cq, pass, 16);
  cl_mem k_out = cli_init_buffer(ctx, cq, out, OUT_LEN);
  cl_mem k_log = cli_init_buffer(ctx, cq, NULL, BUF_SIZE);

  clSetKernelArg(kernel, 0, sizeof(cl_mem), &k_salt);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &k_cipher);
  clSetKernelArg(kernel, 2, sizeof(cl_mem), &k_base);
  clSetKernelArg(kernel, 3, sizeof(cl_mem), &k_pass);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), &k_out);
  clSetKernelArg(kernel, 5, sizeof(cl_mem), &k_log);
  clSetKernelArg(kernel, 6, sizeof(int), &k_count);

  size_t local;
  clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE,
                           sizeof(local), &local, NULL);

  size_t global = count / COUNT;
  clEnqueueNDRangeKernel(cq, kernel, 1, NULL, &global, &local, 0, NULL, NULL);

  printf("  %zu threads (each %d job), local=%zu\n", global, COUNT, local);
  clEnqueueReadBuffer(cq, k_out, CL_TRUE, 0, OUT_LEN, out, 0, NULL, NULL);
  clEnqueueReadBuffer(cq, k_log, CL_TRUE, 0, BUF_SIZE, log, 0, NULL, NULL);
  clFinish(cq);

  // clear up
  clReleaseMemObject(k_salt);
  clReleaseMemObject(k_cipher);
  clReleaseMemObject(k_base);
  clReleaseMemObject(k_pass);
  clReleaseMemObject(k_log);
  clReleaseMemObject(k_out);

  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(cq);
  clReleaseContext(ctx);

  return 0;
}