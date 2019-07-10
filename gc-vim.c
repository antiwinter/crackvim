#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#if !defined(USE_CL)
#define uchar uint8_t
#define ushort uint16_t
#define uint uint32_t
#endif

#define BASE "abcdefghijklmnopqrstuvwxyz_1234567890."
#define GROUP (1 << 22)
#define PASS_MAX 16
#define THREAD_MAX 100

void init_salt(uint32_t salt[]) {
  uint32_t n, k, c;

  for (n = 0; n < 256; n++) {
    c = n;
    for (k = 0; k < 8; k++) {
      if (c & 1)
        c = 0xedb88320L ^ (c >> 1);
      else
        c = c >> 1;
    }
    salt[n] = c;
  }
}

void update_key(uint salt[], uint key[3], uchar c) {
  key[0] = salt[(key[0] ^ c) & 0xff] ^ (key[0] >> 8);
  key[1] = key[1] + (key[0] & 0xff);
  key[1] = key[1] * 134775813 + 1;
  key[2] = salt[(key[2] ^ (key[1] >> 24)) & 0xff] ^ (key[2] >> 8);
}

void update_pass(char pass[], int inc, uchar *base) {
  char *p = pass;
  int len = *(uint *)base, n, c = 0;
  char *set = (char *)(base + 4);
  char *rs = (char *)(base + 64);

  do {
    n = *p ? rs[*p] : 0;
    n += inc + c;
    c = n / len;
    *p++ = set[n % len];
    inc = 0;
  } while (c || *p);

  *p = 0;
}

int dec_u8(uchar *cipher, uint salt[], uint key[]) {
  uchar *p = cipher, x, c = 0, n;

  for (; *p;) {
    // dec cipher
    ushort t = key[2] | 2;
    x = *p++ ^ ((t * (t ^ 1)) >> 8);

    // check if u8
    if (c) {
      if (((x >> 6) & 3) != 2) return 0;
      c--;
    } else {
      if (x & 0x80) {
        for (n = x; n & 0x80; c++, n <<= 1)
          ;
        if (c < 2 || c > 3) return 0;
        c--;
      }
    }

    // next
    update_key(salt, key, x);
  }

  return 1;
}

struct fiber_params {
  uint *salt;
  char pass[PASS_MAX];
  int n;  // count of passwords
  char *out;
  uchar *cipher;
  uchar *base;
  pthread_t pid;
  int id;
};

void *fiber(void *input) {
  struct fiber_params *fp = (struct fiber_params *)input;
  int cursor = 0;

  update_pass(fp->pass, fp->id * fp->n, fp->base);
  // sprintf(fp->pass, "lenin");
  *fp->out = 0;
  for (; fp->n--; update_pass(fp->pass, 1, fp->base)) {
    uint key[3] = {305419896, 591751049, 878082192};
    char *p = fp->pass, *q;
    for (; *p;) update_key(fp->salt, key, *p++);

    if (dec_u8(fp->cipher, fp->salt, key)) {
      p = fp->pass;
      q = &fp->out[cursor];
      cursor += PASS_MAX;
      fp->out[cursor] = 0;
      do {
        *q++ = *p++;
      } while (*p);
    }
  }

  return NULL;
}

void run_fibers(uint *salt, uchar *cipher, uchar *base, char *pass, int count,
                int threads) {
  int i, each = (count + threads - 1) / threads;
  struct fiber_params fp[THREAD_MAX];

  for (i = 0; i < threads; i++) {
    fp[i].salt = salt;
    fp[i].cipher = cipher;
    fp[i].base = base;
    fp[i].n = each;
    fp[i].id = i;
    strncpy(fp[i].pass, pass, PASS_MAX);
    fp[i].out = malloc(each * PASS_MAX + 4);
    pthread_create(&fp[i].pid, NULL, fiber, &fp[i]);
  }

  for (i = 0; i < threads; i++) {
    char *p = fp[i].out;
    pthread_join(fp[i].pid, NULL);
    for (; *p; p += PASS_MAX) {
      printf("%s\n", p);  // possible solution
    }
    free(fp[i].out);
  }
}

void run_fibers_cl() {}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("no file provided\n");
    return 0;
  }

  // get threads info
  char *th = getenv("TH");
  int i, tn = th ? atoi(th) : 1;

  // init cipher
  uint8_t cipher[1048];
  int fd = open(argv[1], O_RDONLY);
  read(fd, cipher, 12);
  int len = read(fd, cipher, 1000);
  cipher[len] = 0;
  close(fd);

  // init salt
  uint32_t salt[256];
  init_salt(salt);

  // init set
  uint8_t base[512], *rs = base + 64, *p = base + 4;
  int *bl = (int *)base;
  sprintf((char *)base + 4, BASE);
  for (*bl = 0; *p; p++, (*bl)++) rs[*p] = *bl;

  // start
  struct timeval t0, t1;
  gettimeofday(&t0, NULL);

  double ai = 0, hi, sp;
  uint8_t *u = (uint8_t *)" kmb";
  char pass[PASS_MAX] = {0};

  for (;; ai += GROUP) {
    run_fibers(salt, cipher, base, pass, GROUP, tn);

    gettimeofday(&t1, NULL);
#define get_unit(_x) for (p = u; _x > 1000 && *p != 'b'; _x /= 1000, p++)
    hi = ai;
    sp = hi / ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) *
         1000000;
    get_unit(hi);
    fprintf(stderr, "\r%.1f%c tested, ", hi, *p);
    get_unit(sp);
    fprintf(stderr, "%.1f%c words/s :: %s   ", sp, *p, pass);
#undef get_unit

    fflush(stderr);
    update_pass(pass, GROUP, base);
  }
}
