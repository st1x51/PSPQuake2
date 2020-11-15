#include <pspkernel.h>
#include <pspwlan.h>
#include <pspsdk.h>
#include <pspnet.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <psputility.h>

#define ADHOC_NET 29

typedef struct sockaddr_adhoc
{
        char   len;
        short family;
        u16	port;
        char mac[6];
	char zero[6];
} sockaddr_adhoc;

int   Adhoc_Init         (void);
void  Adhoc_Term         (void);
void  Adhoc_Shutdown     (void);
int   Adhoc_OpenSocket   (int port);
int   Adhoc_CloseSocket  (int socket);
int   Adhoc_Recv         (int socket, char *buf, int len, struct sockaddr *addr);
int   Adhoc_Send         (int socket, char *buf, int len, struct sockaddr *addr);
char *Adhoc_AddrToString (struct sockaddr *addr);
int   Adhoc_StringToAddr (char *string, struct sockaddr *addr);
int   Adhoc_AddrCompare  (struct sockaddr *addr1, struct sockaddr *addr2);

