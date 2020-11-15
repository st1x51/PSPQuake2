/*
Copyright (C) 2010 Crow_bar.
*/

#ifndef Q2K_H
#define Q2K_H

int Q2KernelInit(int kb);
int Q2KernelSetVideoOut(int u, int mode, int width, int height, int x, int y, int z);
int Q2KernelCheckVideoOut();
#endif
