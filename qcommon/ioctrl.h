#ifndef IOCTRL_H
#define IOCTRL_H

int FS_Fopen(const char* name, const char* mode);
int FS_Fclose(int index);
int FS_Fseek(int index, long pos, int func);
int FS_Ftell(int index);
int FS_Fread(void* buffer, int off, int length, int index);
int FS_Fwrite(const void* buffer, int size, int n, int index);
int FS_Fscanf(int index, const char *str, ...);
int FS_Fprintf(int index, const char *str, ...);
int FS_Fflush(int index);
FILE *FS_FGetFile(int index);
void FS_Suspend(void);
void FS_Resume(void);
#endif //IOCTRL_H

