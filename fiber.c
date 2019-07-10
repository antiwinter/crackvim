
#ifndef __OPENCL_VERSION__
#define uchar unsigned char
#define uint unsigned int
#define ushort unsigned short
#define __kernel
#define __private
#define __global
static inline int atomic_add(volatile int *a, int v) {
  int t = *a;
  *a += v;
  return t;
}
#define putn(...)
#else
#endif

#define PASS_MAX 16

void update_key(uint *salt, uint key[3], uchar c) {
  key[0] = salt[(key[0] ^ c) & 0xff] ^ (key[0] >> 8);
  key[1] = key[1] + (key[0] & 0xff);
  key[1] = key[1] * 134775813 + 1;
  key[2] = salt[(key[2] ^ (key[1] >> 24)) & 0xff] ^ (key[2] >> 8);
}

void update_pass(uchar *pass, int inc, uchar *base) {
  uchar *p = pass;
  int len = *(uint *)base, n, c = 0;
  uchar *set = (uchar *)(base + 4);
  uchar *rs = (uchar *)(base + 64);

  do {
    n = *p ? rs[*p] : 0;
    n += inc + c;
    c = n / len;
    *p++ = set[n % len];
    inc = 0;
  } while (c || *p);

  *p = 0;
}

int dec_u8(uchar *cipher, uint *salt, uchar *pass, uchar *txt) {
  uint key[3] = {305419896, 591751049, 878082192};
  uchar *p = pass, x, c = 0, n, *q = txt;
  for (; *p;) update_key(salt, key, *p++);

  for (p = cipher; *p;) {
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

    // save decoded
    if (txt) {
      *q++ = x;
      *q = 0;
    }

    // next
    update_key(salt, key, x);
  }

  return 1;
}

__kernel void _fiber(
#ifndef __OPENCL_VERSION__
    uint *salt, uchar *cipher, uchar *base, uchar *pass, uchar *out,
    const int count, const int id
#else
    __global uint *g_salt, __global uchar *g_cipher, __global uchar *g_base,
    __global uchar *g_pass, volatile __global uchar *out, __global uchar *log,
    const int count
#endif
) {
  int n = count;

#ifdef __OPENCL_VERSION__
#define get(_t, _v, l)                  \
  _t _v[l];                             \
  do {                                  \
    _t *_p = _v;                        \
    __global _t *_q = g_##_v;           \
    int _i = 0;                         \
    for (; _i < l; _i++) *_p++ = *_q++; \
  } while (0)

  get(uint, salt, 256);
  get(uchar, cipher, 1048);
  get(uchar, base, 512);
  get(uchar, pass, 16);
#undef get

#define puts(_s)                     \
  do {                               \
    __constant char *__s = _s;       \
    int _i = 0;                      \
    do {                             \
      *log++ = *__s++;               \
      _i++;                          \
    } while (*__s);                  \
    for (; _i & 3; _i++) *log++ = 0; \
  } while (0)

#define putn(_p, _n)                       \
  do {                                     \
    uchar *__p = (uchar *)_p;              \
    int _i = 0;                            \
    for (; _i < _n; _i++) *log++ = *__p++; \
  } while (0)

  int id = get_global_id(0);
#endif

  // puts("hello cl");
  // putn(&id, 4);

  // puts("count");
  // putn(&count, 4);

  update_pass(pass, id * count, base);
  *(out + 4) = 0;

  for (; n--; update_pass(pass, 1, base)) {
    if (dec_u8(cipher, salt, pass, 0)) {
      uchar *p;
      volatile __global uchar *q;
      int cursor = atomic_add((volatile __global int *)out, PASS_MAX);
      p = pass;
      q = &out[cursor];
      cursor += PASS_MAX;
      out[cursor] = 0;
      do {
        *q++ = *p;
      } while (*p++);
    }
  }
}
