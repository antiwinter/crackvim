#ifndef __GCV_H__
#define __GCV_H__

#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define BASE "abcdefghijklmnopqrstuvwxyzCLETRWDS_1234567890."
#define GROUP (1 << 23)
#define OUT_LEN (GROUP >> 7)
#define PASS_MAX 16
#define THREAD_MAX 100
#define MSG_MAX 1048

extern int cl_init(uint32_t *salt, uint8_t *cipher, uint8_t *base, int count);
extern int run_fibers_cl(uint8_t *pass, uint8_t *out);

#endif
