
#include <stdlib.h>
#include <errno.h>
#include "util/portability.hh"

#ifdef WIN32

int RUSAGE_SELF = 0;

int sysconf(int) { return 0; }
int msync(void*, int, int) { return 0; }
int munmap(void *, int) { return 0; }
void *mmap(void*, int, int, int, FD, OFF_T) { return 0; }
int write(int, const void *, int) {return 0; }

//FILE *popen(const char*, const char*) { return 0; }
//int pclose(FILE *) { return 0; }
int close(FD fd) { return 0; }


// to be implemented by boost
int mkdtemp(const char*) { return 0; }

// done
long lrint(float x)
{
  long ret = (long) x;
  return ret;
}

float strtof(const char *begin, char **end) 
{ 
	double ret = strtod(begin, end);
	return (float) ret; 
}

#endif 


