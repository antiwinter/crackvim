#define __CL_ENABLE_EXCEPTIONS
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

void run_fibers_cl(uint32_t *salt, uint8_t *cipher, uint8_t *base,
                   uint8_t *pass, int count) {
  int err, fd, len, n_found = 0, k_count = COUNT, cu_n;
  char src[8192], name[64];

  fd = open("./fiber.c", O_RDONLY);
  len = read(fd, src, 8192);
  src[len] = 0;
  close(fd);

  int out_len = count / 128 * 16 + 4;
  uint8_t *out = malloc(out_len);

  // prepare device
  cl_device_id device_id;
  clGetDeviceIDs(NULL, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
  clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, 4, &cu_n, NULL);
  clGetDeviceInfo(device_id, CL_DEVICE_NAME, 64, name, NULL);

  printf("using %s\n", name);
  printf("  %d CUs\n", cu_n);

  // prepare kernel
  cl_context ctx = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
  cl_command_queue cq = clCreateCommandQueue(ctx, device_id, 0, &err);
  cl_program program =
      clCreateProgramWithSource(ctx, 1, (const char **)&src, NULL, &err);
  clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  cl_kernel kernel = clCreateKernel(program, "fiber", &err);

  // prepare arguments
  cl_mem k_salt = cli_init_buffer(ctx, cq, salt, sizeof(uint32_t) * 256);
  cl_mem k_cipher = cli_init_buffer(ctx, cq, cipher, 1048);
  cl_mem k_base = cli_init_buffer(ctx, cq, base, 512);
  cl_mem k_pass = cli_init_buffer(ctx, cq, pass, 16);
  cl_mem k_nfound = cli_init_buffer(ctx, cq, &n_found, 4);
  cl_mem k_out = cli_init_buffer(ctx, cq, NULL, 0);

  clSetKernelArg(kernel, 0, sizeof(cl_mem), &k_salt);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &k_cipher);
  clSetKernelArg(kernel, 2, sizeof(cl_mem), &k_base);
  clSetKernelArg(kernel, 3, sizeof(cl_mem), &k_pass);
  clSetKernelArg(kernel, 4, sizeof(cl_mem), &k_out);
  clSetKernelArg(kernel, 5, sizeof(cl_mem), &k_nfound);
  clSetKernelArg(kernel, 6, sizeof(int), &k_count);

  size_t local;
  clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE,
                           sizeof(local), &local, NULL);

  size_t global = count / COUNT;
  clEnqueueNDRangeKernel(cq, kernel, 1, NULL, &global, &local, 0, NULL, NULL);

  printf("  %d threads (each %d job), local=%d\n", global, COUNT, local);
  clEnqueueReadBuffer(cq, k_out, CL_TRUE, 0, out_len, out, 0, NULL, NULL);
  clEnqueueReadBuffer(cq, k_nfound, CL_TRUE, 0, 4, &n_found, 0, NULL, NULL);
  clFinish(cq);

  if (n_found) {
    // printf("%d found:\n", n_found / 16);
    uint8_t txt[MSG_MAX], _pass[PASS_MAX], *p;
    for (p = out; *p; p += PASS_MAX) {
      strncpy((char *)_pass, (char *)p, PASS_MAX);
      dec_u8(cipher, salt, _pass, txt);
      printf("%s   %s\n", _pass, txt);  // possible solution
    }
  }

  // clear up
  clReleaseMemObject(k_salt);
  clReleaseMemObject(k_cipher);
  clReleaseMemObject(k_base);
  clReleaseMemObject(k_pass);
  clReleaseMemObject(k_nfound);
  clReleaseMemObject(k_out);

  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(cq);
  clReleaseContext(ctx);
  return 0;
}