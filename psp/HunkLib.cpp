extern"C"
{
#include "../qcommon/qcommon.h"
//#include <sys/mman.h>
}

#include <valarray>
#include <vector>
#include <list>
#include <malloc.h>
#include <errno.h>


using namespace std;
list<byte*> memorylist;

#if 1
int		hunkcount = 0;

byte *membase;
int maxhunksize;
int curhunksize;

void *Hunk_Begin (int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
	maxhunksize = maxsize;
	curhunksize = 0;
	membase = (byte*)malloc(maxhunksize);
	memset(membase, 0, maxhunksize);
	if (!membase)
	{
 		printf("Hunk Error\n");
		Sys_Error("Unable to allocate %i bytes", maxsize);
    }

	return (void *)membase;
}

void *Hunk_Alloc (int size)
{
	byte *buf;
	// round to cacheline
	size = (size+31)&~31;

	if (curhunksize + size > maxhunksize)
		Sys_Error("Hunk_Alloc overflow");
	buf = membase + curhunksize;
	memset(buf, 0, size);
	curhunksize += size;
    
	return buf;
}

int Hunk_End (void)
{
	byte *nrl;
	nrl = (byte*)realloc(membase, curhunksize);

	if (nrl != membase)
	{
		printf("Hunk Error\n");
		Sys_Error("Hunk_End:  Could not remap virtual block (%d)", errno);
    }

	assert(nrl==membase);

	hunkcount++;

    printf ("hunkcount: %i\n", hunkcount);

    memorylist.push_back(membase);

	return curhunksize;
}

void Hunk_Free (void *base)
{
	if (base) 
	{
		free(base);
		base = NULL;
		hunkcount--;
	}

}

void Hunk_FreeAll (void)
{		
    byte *ptr;
    while (memorylist.size() > 0)
	{
		ptr = memorylist.front();
        memorylist.pop_front();
		printf("Free HUNK\n");
	    free(ptr);
	}
}
#else
void Hunk_Begin( mempool_t *pool, int maxsize ) {
    byte *buf;

	// reserve a huge chunk of memory, but don't commit any yet
	pool->maxsize = ( maxsize + 4095 ) & ~4095;
	pool->cursize = 0;
  
	buf = (byte*)malloc(pool->maxsize);
	
	memset(buf, 0, pool->maxsize);
  
	if( buf == NULL || buf == ( byte * )-1 ) 
	{
		Com_Error( ERR_FATAL, "Hunk_Begin: unable to virtual allocate %d bytes",
                pool->maxsize );
  }
  pool->base = buf;
  pool->mapped = pool->maxsize;
}

void *Hunk_Alloc( mempool_t *pool, int size ) 
{
	byte *buf;

	// round to cacheline
	size = ( size + 31 ) & ~31;
	if( pool->cursize + size > pool->maxsize ) 
	{
		Com_Error( ERR_FATAL,
            "Hunk_Alloc: unable to allocate %d bytes out of %d",
                size, pool->maxsize );
  }
	buf = pool->base + pool->cursize;
	pool->cursize += size;
	return buf;
}

void Hunk_End( mempool_t *pool ) 
{
	byte *n;
  n = (byte*)realloc(pool->base, pool->cursize);

	if( n != pool->base ) {
		Com_Error( ERR_FATAL, "Hunk_End: could not remap virtual block: %s",
                strerror( errno ) );
    }
    pool->mapped = pool->cursize;
}

void Hunk_Free( mempool_t *pool ) 
{
	if( pool->base ) 
	{
		free( pool->base);
		memset(pool->base, 0, pool->mapped);
		if(pool->base)
			Com_Error( ERR_FATAL, "Hunk_Free: free failed: %s",strerror( errno ) );
	}
    memset( pool, 0, sizeof( *pool ) );
}
#endif
