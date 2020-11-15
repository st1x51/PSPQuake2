#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

#include "glob.h"

#include "../qcommon/qcommon.h"

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspdebug.h>
#include <psppower.h>
#include <pspsdk.h>

//#define PSPFIND

//===============================================================================

//============================================


/*
================
Sys_Milliseconds
================
*/
int curtime;
int Sys_Milliseconds (void)
{
	struct timeval tp;
	struct timezone tzp;
	static int		secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - secbase)*1000 + tp.tv_usec/1000;

	return curtime;
}

void Sys_Mkdir (char *path)
{
	printf("Mkdir: %s\n", path);
	sceIoMkdir(path, 0x777);
}

char *strlwr (char *s)
{
	char *p = s;
	while (*s) {
		*s = tolower(*s);
		s++;
	}
	return p;
}
//============================================

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
#ifdef PSPFIND  
static SceUID fdir;
#else
static	DIR		*fdir;
#endif
static qboolean CompareAttributes(char *path, char *name, unsigned musthave, unsigned canthave )
{
	struct stat st;
	char fn[MAX_OSPATH];

// . and .. never match
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return false;
 
  return true;
	
	sprintf(fn, "%s/%s", path, name);
	if (stat(fn, &st) == -1)
		return false; // shouldn't happen

	if ( ( st.st_mode & S_IFDIR ) && ( canthave & SFF_SUBDIR ) )
		return false;

	if ( ( musthave & SFF_SUBDIR ) && !( st.st_mode & S_IFDIR ) )
		return false;

	return true;

}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canhave)
{

	char *p;

	if (fdir)
		Sys_Error ("Sys_BeginFind without close");

//	COM_FilePath (path, findbase);
	strcpy(findbase, path);

	if ((p = strrchr(findbase, '/')) != NULL) 
	{
		*p = 0;
		strcpy(findpattern, p + 1);
	} 
	else
		strcpy(findpattern, "*");

	if (strcmp(findpattern, "*.*") == 0)
		strcpy(findpattern, "*");
	
	
#ifdef PSPFIND  

  if((fdir = sceIoDopen(findbase)) == 0) 
	{
		return 0;
	}

	SceIoDirent d;

  memset(&d, 0, sizeof(SceIoDirent));

	while (sceIoDread(fdir, &d) != 0) 
	{
		if (!*findpattern || glob_match(findpattern, strlwr(d.d_name))) 
		{
			if (CompareAttributes(findbase, d.d_name, musthave, canhave)) 
			{
				sprintf (findpath, "%s/%s", findbase, d.d_name);
				printf("find file %s\n",findpath);
				return findpath;
			}
		}
	  memset(&d, 0, sizeof(SceIoDirent));
	}
#else
	struct dirent *d;

	if ((fdir = opendir(findbase)) == NULL)
		return NULL;
  
	while ((d = readdir(fdir)) != NULL) 
	{
		if (!*findpattern || glob_match(findpattern, strlwr(d->d_name))) 
		{
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) 
			{
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				printf("%s\n",d->d_name);
				printf("return to: %s\n",findpath);
				return findpath;
			}
    }
	}
#endif
	return NULL;
}

char *Sys_FindNext (unsigned musthave, unsigned canhave)
{

#ifdef PSPFIND	
	if (fdir == 0)
		return NULL;

	SceIoDirent d;
  memset(&d, 0, sizeof(SceIoDirent));
 	while (sceIoDread(fdir, &d) != 0) 
	{
		if (!*findpattern || glob_match(findpattern, strlwr(d.d_name))) 
		{
			if (CompareAttributes(findbase, d.d_name, musthave, canhave)) 
			{
				sprintf (findpath, "%s/%s", findbase, d.d_name);
				printf("nxfind file %s\n",findpath);
				return findpath;
			}
		}
	  memset(&d, 0, sizeof(SceIoDirent));
	}
#else
  if (fdir == NULL)
		return NULL;
		
	struct dirent *d;
	while ((d = readdir(fdir)) != NULL) 
	{
		if (!*findpattern || glob_match(findpattern, strlwr(d->d_name))) 
		{
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) 
			{
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
#endif	
	return NULL;
}

void Sys_FindClose (void)
{
#ifdef PSPFIND 
	if (fdir != 0)
		sceIoDclose(fdir);
  fdir = 0;
#else	
	if (fdir != NULL)
		closedir(fdir);
  fdir = NULL;
#endif	
	
}

//============================================
