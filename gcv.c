#include "gcv.h"

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

#include "fiber.c"

struct fiber_params {
  uint32_t *salt;
  uint8_t pass[PASS_MAX];
  int n;  // count of passwords
  uint8_t *out;
  uint8_t *cipher;
  uint8_t *base;
  pthread_t pid;
  int *n_found;
  int id;
};

void *fiber(void *input) {
  struct fiber_params *fp = (struct fiber_params *)input;
  _fiber(fp->salt, fp->cipher, fp->base, fp->pass, fp->out, fp->n_found, fp->n,
         fp->id);

  return NULL;
}

int run_fibers(uint32_t *salt, uint8_t *cipher, uint8_t *base, uint8_t *pass,
               uint8_t *out, int count, int threads) {
  int i, each = (count + threads - 1) / threads, n_found = 0;
  struct fiber_params fp[THREAD_MAX];

  for (i = 0; i < threads; i++) {
    fp[i].salt = salt;
    fp[i].cipher = cipher;
    fp[i].base = base;
    fp[i].n = each;
    fp[i].id = i;
    fp[i].n_found = &n_found;
    strncpy((char *)fp[i].pass, (char *)pass, PASS_MAX);
    fp[i].out = out;
    pthread_create(&fp[i].pid, NULL, fiber, &fp[i]);
  }

  for (i = 0; i < threads; i++) pthread_join(fp[i].pid, NULL);
  return n_found;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("no file provided\n");
    return 0;
  }

  // get threads info
  char *th = getenv("TH");
  int i, tn = th ? atoi(th) : 1;

  // init cipher
  uint8_t cipher[MSG_MAX];
  int fd = open(argv[1], O_RDONLY);
  read(fd, cipher, 12);
  int len = read(fd, cipher, MSG_MAX - 48);
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
  uint8_t *u = (uint8_t *)" kmgt";
  uint8_t pass[PASS_MAX] = {0};

  uint8_t *out = malloc(OUT_LEN);
  for (;; ai += GROUP) {
    // int n_found = run_fibers(salt, cipher, base, pass, out, GROUP, tn);
    int n_found = run_fibers_cl(salt, cipher, base, pass, out, GROUP);

    if (n_found < 0) return n_found;
    if (n_found) {
      // printf("%d found:\n", n_found / 16);
      uint8_t txt[MSG_MAX], _pass[PASS_MAX], *p;
      for (p = out; *p; p += PASS_MAX) {
        strncpy((char *)_pass, (char *)p, PASS_MAX);
        dec_u8(cipher, salt, _pass, txt);
        printf("%s   %s\n", _pass,
               txt);  // possible solution
      }
    }
    update_pass(pass, GROUP, base);

    gettimeofday(&t1, NULL);
#define get_unit(_x) for (p = u; _x > 1000 && *(p + 1); _x /= 1000, p++)
    hi = ai + GROUP;
    sp = hi / ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) *
         1000000;
    get_unit(hi);
    fprintf(stderr, "\r%.1f%c tested, ", hi, *p);
    get_unit(sp);
    fprintf(stderr, "%.1f%c words/s :: %s   ", sp, *p, pass);
#undef get_unit

    fflush(stderr);
  }
}
