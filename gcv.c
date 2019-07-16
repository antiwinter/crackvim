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
  int id;
};

void *fiber(void *input) {
  struct fiber_params *fp = (struct fiber_params *)input;
  _fiber(fp->salt, fp->cipher, fp->base, fp->pass, fp->out, fp->n, fp->id);

  return NULL;
}

int run_fibers(uint32_t *salt, uint8_t *cipher, uint8_t *base, uint8_t *pass,
               uint8_t *out, int count, int threads) {
  int i, each = (count + threads - 1) / threads;
  struct fiber_params fp[THREAD_MAX];
  *(uint32_t *)out = 4;

  for (i = 0; i < threads; i++) {
    fp[i].salt = salt;
    fp[i].cipher = cipher;
    fp[i].base = base;
    fp[i].n = each;
    fp[i].id = i;
    strncpy((char *)fp[i].pass, (char *)pass, PASS_MAX);
    fp[i].out = out;
    pthread_create(&fp[i].pid, NULL, fiber, &fp[i]);
  }

  for (i = 0; i < threads; i++) pthread_join(fp[i].pid, NULL);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("no file provided\n");
    return 0;
  }

  // get threads info
  char *env = getenv("TH");
  int i, err, tn = env ? atoi(env) : 0;

  env = getenv("TEST");
  if (env) tn = 1;

#if defined(NOCL)
  tn = tn ? tn : 1;
  env = 0;
#endif

  // init cipher
  uint8_t cipher[MSG_MAX];
  int fd = open(argv[1], O_RDONLY);
  read(fd, cipher, 12);
  int len = read(fd, cipher + 4, MSG_MAX - 48);
  (cipher + 4)[len] = 0;
  *(uint32_t *)cipher = len;
  close(fd);

  // init salt
  uint32_t salt[256];
  init_salt(salt);

  // init set
  uint8_t base[512], *rs = base + 64, *p = base + 4, *q;
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
  uint8_t *out1 = malloc(OUT_LEN);

  FILE *fp = fopen("./.pass", "r");
  if (fp) {
    fscanf(fp, "%s", pass);
    fclose(fp);
    printf("continue searching from %s\n", pass);
  }

  // init device
  if (tn) {
    printf("Using CPU: %d threads\n", tn);
  }

  if (!tn || env) {
#if defined(NOCL)
    err = -99;
#else
    err = cl_init(salt, cipher, base, GROUP);
#endif
    if (err) return err;
  }

  int n_found = 0;
  for (;; ai += GROUP) {
    fflush(stdout);
    if (env || tn) {
      err = run_fibers(salt, cipher, base, pass, out, GROUP, tn);
      if (err) return err;
      p = out;
      q = 0;
    }

    if (env || !tn) {
#if defined(NOCL)
      err = -99;
#else
      err = run_fibers_cl(pass, out1);
#endif
      if (err) return err;
      p = out1;
      q = 0;
    }

    if (env) {
      p = out;
      q = out1;
    }

    // exit(0);
    if (*(uint32_t *)p / 16) {
      n_found += *(uint32_t *)p / 16;
      // printf("%d found:\n", n_found / 16);
      uint8_t txt[MSG_MAX], _pass[PASS_MAX];
      i = 0;
      for (p += 4, q += 4; *p; p += PASS_MAX, q += PASS_MAX) {
        strncpy((char *)_pass, (char *)p, PASS_MAX);
        dec_u8(cipher, salt, _pass, txt);
        if (env) {
          printf("%d/(%d-%d) : %s - %s : %s\n", ++i, *(uint32_t *)out / 16,
                 *(uint32_t *)out1 / 16, p, q,
                 strncmp((char *)p, (char *)q, PASS_MAX) ? "FAIL" : "pass");
        } else
          printf("%s :%lu: %s\n", _pass, strnlen((char *)txt, MSG_MAX),
                 txt);  // possible solution
      }
    }

    fp = fopen("./.pass", "w");
    fprintf(fp, "%s\n", pass);
    fclose(fp);

    update_pass(pass, GROUP, base);
    if (env) continue;

    gettimeofday(&t1, NULL);
#define get_unit(_x) for (p = u; _x > 1000 && *(p + 1); _x /= 1000, p++)
    hi = ai + GROUP;
    sp = hi / ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) *
         1000000;
    get_unit(hi);
    fprintf(stderr, "\r%.1f%c tested (%d), ", hi, *p, n_found);
    get_unit(sp);
    fprintf(stderr, "%.1f%c words/s :: %s   ", sp, *p, pass);
#undef get_unit

    fflush(stderr);
  }
}
