#include <iostream>
#include <thread>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

uint32_t _salt[256];
uint8_t _cipher[1048];

void init_salt(void) {
    uint32_t n, k, c;

    for (n = 0; n < 256; n++) {
        c = n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        _salt[n] = c;
    }
}

class dec {
    private:
        uint32_t key[4];
        void update(uint8_t c);

    public:
        void reset(char *p);
        int test();
        char text[1048];
};

void dec::update(uint8_t c) {
    key[0] = _salt[(key[0] ^ c) & 0xff] ^ (key[0]>>8);
    key[1] = key[1] + (key[0] & 0xff);
    key[1] = key[1] * 134775813 + 1;
    key[2] = _salt[(key[2] ^ (key[1] >> 24)) & 0xff] ^ (key[2]>>8);
}

void dec::reset(char *p) {
    key[0] = 305419896;
    key[1] = 591751049;
    key[2] = 878082192;

    while(*p) update(*p++);
}

int dec::test(void) {
    uint8_t *p = _cipher, x, c = 0, n;
    char *q = text; 

    for(; *p;) {
        // dec cipher
        uint16_t t = key[2] | 2;
        x = *p++ ^ ((t * (t ^ 1)) >> 8);

        // check if u8
        if (c) {
            if (((x >> 6) & 3) != 2) return 0;
            c--;
        } else {
            if (x & 0x80) {
                for (n = x; n & 0x80; c++, n <<= 1);
                if (c < 2 || c > 3) return 0;
                c--;
            }
        }

        *q++ = x; *q = 0;
        // next
        update(x);
    }

    return 1;
}

const char *_set = "abcdefghijklmnopqrstuvwxyz_1234567890";
int _len = 0;
int _rs[256];

class fiber {
    private:
        int id;
        int interval;
    public:
        class dec dec;
        void next(void);
        std::thread th;
        char pass[32];
        uint64_t count;
        void start(int id, int val);
};

void fiber::next(void) {
    char *p = pass, c = 0, n;
    int inc = *p ? interval: this->id;

    do {
        n = *p ? _rs[*p]: 0;
        n += inc + c;
        c = n / _len;
        *p++ = _set[n % _len];
        inc = 0;
    } while(c || *p);

    *p = 0;
}

void fiber_loop(class fiber *fb) {
loop:

    fb->dec.reset(fb->pass);
    if (fb->dec.test()) {
        printf("possible solution %s  :::  %s\n", fb->pass, fb->dec.text);
    }
    fb->next();
    goto loop;
}

void fiber::start(int id, int val) {
    this->id = id;
    interval = val;
    pass[0] = 0;

    next();
    th = std::thread(fiber_loop, this);
}


int main(int argc, char *argv[]) {
    int tn = 0, i;
    char *th = getenv("TH");
    if(!th) tn = 1; else tn = atoi(th);

    if (argc < 2) {
        std::cout << "no file provided" << std::endl;
        return 0;
    }

    // init cipher
    int fd = open(argv[1], O_RDONLY);
    int len = read(fd, _cipher, 1000);
    _cipher[len] = 0;
    close(fd);

    // init salt
    init_salt();

    // start fibers on CPU
    class fiber *fb[100];
    for (i = 0; i < tn; i++) {
        fb[i] = new class fiber;
        fb[i]->start(i, tn);
    }
    fb[i] = NULL;
}

