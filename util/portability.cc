
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


int ftruncate (FD hfile, unsigned int size)
{
  unsigned int curpos;
  /*
  HANDLE hfile;

  if (fd < 0)
    {
      errno = EBADF;
      return -1;
    }

  hfile = (HANDLE) _get_osfhandle (fd);
  */
  curpos = SetFilePointer (hfile, 0, NULL, FILE_CURRENT);
  if (curpos == ~0
      || SetFilePointer (hfile, size, NULL, FILE_BEGIN) == ~0
      || !SetEndOfFile (hfile))
    {
      int error = GetLastError (); 
      switch (error)
	{
	case ERROR_INVALID_HANDLE:
	  errno = EBADF;
	  break;
	default:
	  errno = EIO;
	  break;
	}
      return -1;
    }
  return 0;
}

#endif 


