/*
 * Copyright 2016 Joseph Landry All Rights Reserved
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "pkzip_crypto.h"
#include "crc32.h"

#define CHECK_LEN 40

void die(char *reason){
	fflush(stdout);
	fputs(reason, stderr);
	fputs("\n", stderr);
	exit(1);
}

void load_file(char *filename, uint8_t **filedata, long *filesize){
	FILE *f;
	long size;
	uint8_t *data;

	f = fopen(filename, "r");
	if(f != NULL){
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);

		if(0 < size){
			data = malloc(size);
			if(data){
				if(fread(data, size, 1, f) == 1){
					fclose(f);
					*filedata = data;
					*filesize = size;
				} else {
					die("error reading file");
				}
			} else {
				die("error allocating");
			}
		} else {
			die("error getting file size");
		}
	} else {
		die("error opening file");
	}

	if(memcmp(data, "VimCrypt~", 9) != 0){
		die("input file is not VimCrypt");
	}

	if(memcmp(data, "VimCrypt~01!", 12) != 0){
		die("input file is not VimCrypt type 01 (PKZIP)");
	}
}


char *_set = "abcdefghijklmnopqrstuvwxyz_1234567890";
int _len = 0;
int _rs[256];

int inc_password(char *password, int inc){
    char *p = password, c = 0, n;

//    printf("inc %s\n", p);
    do {
        n = *p ? _rs[*p]: 0;
//        printf("  %c:%d\n", *p, _rs[*p]);
        n += inc + c;
        c = n / _len;
        *p++ = _set[n % _len];
        inc = 0;
//        printf("  new: %c\n", _set[n % _len]);
//        printf("  car: %d\n", c);
    } while(c || *p);

    *p = 0;

    return 1;
}

int is_u8(char *data, int len) {
    int i = 0, c = 0;
    char *p = data;

    for (;i < len; i++, p++) {
        if(c) {
            if (*p >> 6 != 2)
                return 0;
            c--;
        } else {
            if (*p & 0x80) {
                char n = *p;
                for(; n & 0x80; c++, n <<= 1);
                if (c == 8) return 0;
            }
        }
    }
    return 1;
}

int crack(uint8_t *ciphertext, long length, long long *counter, int inc, char *start_passwd, FILE *dict){
	char *password = start_passwd;
	char *newline;
	char *plaintext;
	uint32_t key[3];
	long i;
	int running = 1;
        char *crib = NULL;

#if 0
	if(start_passwd){
		strncpy(password, start_passwd, sizeof(password));
	}
	password[sizeof(password)-1] = 0;
#endif

	plaintext = malloc(length+1);
	if(plaintext == NULL){
		die("error allocating memory");
	}

	if(dict){
		if(fgets(password, sizeof(password), dict) == NULL){
			die("error reading from dictionary file");
		}
		newline = strchr(password, '\n');
		if(newline){
			*newline = 0;
		}
	}

	while(running){
		init_key(key, password);

		pkzip_decrypt(key, ciphertext, length, (uint8_t *)plaintext);
		plaintext[length] = 0;
		if(crib != NULL){
			if(strstr(plaintext, crib) != NULL){
				printf("Possible password: '%s'\n", password);
				printf("Plaintext: %-32s\n", plaintext);
			}
		} else {
                    if(is_u8(plaintext, length < CHECK_LEN? length: CHECK_LEN)){
				printf("uossible password: '%s'\n", password);
				printf("ulaintext: %-32s\n", plaintext);
                                exit(0);
			}
		}
		if(dict){
			if(fgets(password, sizeof(password), dict) == NULL){
				running = 0;
			}
			newline = strchr(password, '\n');
			if(newline){
				*newline = 0;
			}
		} else {
			running = inc_password(password, inc);
                        *counter += running;
		}
	}

	return 0;
}

struct fiber_info {
    uint8_t data[1024];
    int len;
    int inc;
    char start[256];
    pthread_t id;
    long long counter;
};

void *fiber(void *x) {
    struct fiber_info *info = x;
    crack(info->data, info->len, &info->counter, info->inc, info->start, NULL);
    return 0;
}

void start_fibers(uint8_t *data, int len, int count)
{
    int i, _i;
    char *p, *u = " kmb";
    struct fiber_info info[100];
    double hi;

    for(i = 0; i < count; i++) {
        memcpy(info[i].data, data, len);
        info[i].len = len;
        info[i].inc = count;

#if 0
        p = info[i].start;
        _i = i;
        while(_i >= 0) {
            *p++ = _set[_i % _len];
            _i -= _len;
        }
        *p = 0;
#endif

        info[i].counter = 0;
        inc_password(info[i].start, i);

        pthread_create(&info[i].id, NULL, fiber, &info[i]);

        printf("thread created\n");
    }

    printf("%d threads running... \n", count);

    for(;;) {
        hi = 0;
        for (i = 0; i < count; i++) {
            hi += info[i].counter;
        }

        for (p = u; hi > 1000 && *p != 'b'; hi /= 1000, p++);
        fprintf(stderr, "\r%.1f%c tested :: %s        ", hi, *p, info[0].start);
        fflush(stderr);
        usleep(500000);
    }
}

void help(){
	fprintf(stderr, "crackvim: [options] [filename]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-b nbytes (default: 128)\n");
	fprintf(stderr, "\t-d dict_file\n");
	fprintf(stderr, "\t-p start_password (default: emty string)\n");
	fprintf(stderr, "\t-C thread count (default: 1)\n");
	fprintf(stderr, "\t-l max_passwd_len (default: 6)\n");
	fprintf(stderr, "\t-c crib\n");
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[]){
	char *filename;
	char *crib = NULL;
	uint8_t *filedata;
	long filesize;
	int max_len = 6;
	int threads = 1;
	char *start_passwd = NULL;
	char *dict_filename;
	FILE *dict = NULL;
	int nbytes = 128;

	if(argc < 2){
		help();
		exit(1);
	} else {
		argv++; argc--;
		while(0 < argc){
			if(strcmp(argv[0], "-c") == 0){
				argc--; argv++;
				if(0 < argc){
					crib = argv[0];
					argc--; argv++;
				} else {
					fprintf(stderr, "\t-c [crib]\n\n");
					fprintf(stderr, "Only report plaintexts containing crib.\n");
					fprintf(stderr, "Without a crib, crackvim will report any plaintext that looks like an ascii text file\n\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-l") == 0){
				argc--; argv++;
				if(0 < argc){
					max_len = atoi(argv[0]);
					argc--; argv++;
				} else {
					fprintf(stderr, "\t-l [max_passwd_len]\n\n");
					fprintf(stderr, "Only test password up to length.  Default: 6\n\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-C") == 0){
				argc--; argv++;
				if(0 < argc){
					threads = atoi(argv[0]);
					argc--; argv++;
				} else {
					fprintf(stderr, "\t-c [threads]\n\n");
					fprintf(stderr, "Character set for password generation\n");
					fprintf(stderr, "\t0: lower alpha (Default)\n");
					fprintf(stderr, "\t1: upper alpha\n");
					fprintf(stderr, "\t2: alpha\n");
					fprintf(stderr, "\t3: alphanum\n");
					fprintf(stderr, "\t4: ascii 0x20 - 0x7e\n");
					fprintf(stderr, "\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-p") == 0){
				argc--; argv++;
				if(0 < argc){
					start_passwd = argv[0];
					argc--; argv++;
				} else {
					fprintf(stderr, "\t-p [start_password]\n\n");
					fprintf(stderr, "Choose a password to start with.\n");
					fprintf(stderr, "Default value is the empty string.\n");
					fprintf(stderr, "This feature can be used to 'resume' an attack at a specific point\n");
					fprintf(stderr, "\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-d") == 0){
				argc--; argv++;
				if(0 < argc){
					dict_filename = argv[0];
					argc--; argv++;
					if(strcmp(dict_filename, "-") == 0){
						dict = stdin;
					} else {
						dict = fopen(dict_filename, "r");
						if(dict == NULL){
							die("error opening dictionary file");
						}
					}
				} else {
					fprintf(stderr, "\t-d [dictionary file]\n\n");
					fprintf(stderr, "Dictionary attack.  Use \"-d -\" for stdin\n\n");
					exit(1);
				}
			} else if(strcmp(argv[0], "-b") == 0){
				argc--; argv++;
				if(0 < argc){
					nbytes = atoi(argv[0]);
					argc--; argv++;
					if(nbytes < 0){
						die("nbytes must be positive integer or zero");
					}
				} else {
					fprintf(stderr, "\t-b [nbytes]\n\n");
					fprintf(stderr, "only analyze first nbytes of file.\n");
					fprintf(stderr, "the value 0 means analyze whole file\n");
					exit(1);
				}
			} else {
				break;
			}
		}
		if(0 < argc){
			filename = argv[0];
		} else {
			printf("filename missing\n");
			exit(1);
		}
	}

	load_file(filename, &filedata, &filesize);
	printf("loaded %s: %ld bytes\n", filename, filesize);
	if(crib){
		printf("searching for crib: \"%s\"\n", crib);
	} else {
		printf("searching for ascii text files\n");
	}
	if(dict){
		printf("using dictionary file: %s\n", dict_filename);
	} else {
		printf("using brute force\n");
		printf("max password length: %d\n", max_len);
		printf("threads: %d\n", threads);
		if(start_passwd){
			printf("starting with password: %s\n", start_passwd);
		}
	}
	printf("\n");
	make_crc_table();

	if(nbytes == 0 || nbytes > filesize - 12){
		nbytes = filesize - 12;
	}

        char *p = _set;
        for(; *p; p++, _len++)
            _rs[*p] = _len;

        start_fibers(filedata+12, nbytes, threads);
	return 0;
}
