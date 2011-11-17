
#pragma once

#include <assert.h>
#include <stdint.h>

#ifdef WIN32

#include <windows.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "util/getopt.hh"

#undef max
#undef min

typedef HANDLE FD;

const FD kBadFD = INVALID_HANDLE_VALUE;

typedef int ssize_t;

#define _SC_PAGE_SIZE 1
#define MS_SYNC 1

int sysconf(int);
int msync(void*, int, int);

long lrint(float); 

//inline int getrusage(int, struct rusage*) { return 0; }
//extern int RUSAGE_SELF;


int mkdtemp(const char*);
int munmap(void *, int);
void *mmap(void*, int, int, int, FD, OFF_T);

#define PROT_READ 1
#define PROT_WRITE 1
#define MAP_FAILED (void*) 0x1
#define MAP_SHARED 1
#define MAP_ANON 1
#define MAP_PRIVATE 1
#define S_IRUSR 1
#define S_IROTH 1
#define S_IRGRP 1

int write(int, const void *, int);
#define S_IRUSR 1
#define S_IWUSR 1

//const char *strerror_r(int, const char *buf, int);

float strtof(const char *begin, char **end);
//FILE *popen(const char*, const char*);
//int pclose(FILE *);
int close(FD fd);

#define dup(x) _dup(x)
#define rmdir(x) _rmdir(x)
#define strerror_r(errNum, buffer, numberOfElements)  strerror_s(buffer, numberOfElements);

#else // assume UNIX OS

#include <stdint.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int FD;
const FD kBadFD = -1;

typedef off_t OFF_T;

#endif
