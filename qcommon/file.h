/*
Copyright (C) 2007 Peter Mackay.

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

// file.h -- platform agnostic substitutions for FILE* functions.

#ifndef FILE_H
#define FILE_H

#include <stddef.h>

typedef struct file_s file_t;

typedef enum file_open_mode_e
{
	FILE_OPEN_READ,
	FILE_OPEN_WRITE,
	FILE_OPEN_APPEND
} file_open_mode_t;

file_t* File_Open(const char* path, file_open_mode_t mode);
void File_Close(file_t* handle);
int File_Seek(file_t* handle, long offset, int origin);
long File_Tell(file_t* handle);
size_t File_Read(void* destination, size_t element_size, size_t count, file_t* handle);
size_t File_Write(const void* source, size_t element_size, size_t count, file_t* handle);
int File_Flush(file_t* handle);
void File_PrintF(file_t* handle, const char* format, ...);

#endif
