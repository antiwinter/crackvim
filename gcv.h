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

#define BASE "abcdefghijklmnopqrstuvwxyz_1234567890."
#define GROUP (1 << 22)
#define PASS_MAX 16
#define THREAD_MAX 100
#define MSG_MAX 1048

#endif
