/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Quake2 you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/
#include "../ref_gl/gl_local.h"
#include "glw_psp.h"
#include <gl/gl.h>



void ( * qglColorTableEXT)( int, int, int, int, int, const void * ) = 0;
void ( * qglActiveTextureARB) ( GLenum ) = 0;
void ( * qglSelectTextureSGIS)( GLenum ) = 0;
void ( * qglLockArraysEXT)( int, int) = 0;
void ( * qglUnlockArraysEXT) ( void ) = 0;
void ( * qglPointParameterfEXT)( GLenum param, GLfloat value ) = 0;
void ( * qglPointParameterfvEXT)( GLenum param, const GLfloat *value ) = 0;
void ( * qglMTexCoord2fSGIS)( GLenum, GLfloat, GLfloat ) = 0;
void ( * qglClientActiveTextureARB) ( GLenum ) = 0;

qboolean QGL_Init( const char *dllname )
{
	  gl_config.allow_cds = true;
    return true;
}

void QGL_Shutdown( void )
{
    qglColorTableEXT = 0;
    qglActiveTextureARB = 0;
    qglSelectTextureSGIS = 0;
    qglLockArraysEXT = 0;
    qglUnlockArraysEXT = 0;
    qglPointParameterfEXT = 0;
    qglPointParameterfvEXT = 0; 
    qglMTexCoord2fSGIS = 0;
    qglClientActiveTextureARB = 0;
}

void GLimp_EnableLogging( qboolean enable )
{
}


void GLimp_LogNewFrame( void )
{
}


