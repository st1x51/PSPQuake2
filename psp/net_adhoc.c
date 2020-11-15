
#include "../qcommon/qcommon.h"

#include <netinet/in.h>

#include "net_adhoc.h"
#include <string.h>

int libReady = 0;

static int pspSdkAdhocInit(char *product)
{
	u32 retVal;
	struct productStruct temp;

    retVal = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
    if (retVal != 0)
        return retVal;

    retVal = sceNetAdhocInit();
    if (retVal != 0)
        return retVal;

	strcpy(temp.product, product);
	temp.unknown = 0;
	
    retVal = sceNetAdhocctlInit(0x2000, 0x20, &temp);
    if (retVal != 0)
        return retVal;

	return 0;
}

static void pspSdkAdhocTerm(void)
{
	sceNetAdhocctlTerm();
	sceNetAdhocTerm();
	sceNetTerm();
}
int Adhoc_OpenSocket(int port)
{
	u8 mac[8];
	sceWlanGetEtherAddr(mac);
	int rc = sceNetAdhocPdpCreate(mac, port, 0x2000, 0);
	return rc;
}

int Adhoc_CloseSocket(int socket)
{
	sceNetAdhocPdpDelete(socket, 0);
	return -1;
}

int Adhoc_Recv(int socket, char *buf, int len, struct sockaddr *addr)
{
	unsigned short port;
	int ret;

	sceKernelDelayThread(1);

	ret = sceNetAdhocPdpRecv(socket, (unsigned char *)((sockaddr_adhoc *)addr)->mac, &port, buf, &len, 0, 1);

	if(ret == 0x80410709)
	   return 0; 

	if(ret < -1)
	   Com_Printf("WLAN: error recv 0x%08X",ret);

	((sockaddr_adhoc *)addr)->port = port;

	return len;
}

int Adhoc_Send(int socket, char *buf, int len, struct sockaddr *addr)
{
	int ret = -1;
	sceKernelDelayThread(1);
	ret = sceNetAdhocPdpSend(socket, (unsigned char*)((sockaddr_adhoc *)addr)->mac, ((sockaddr_adhoc *)addr)->port, buf, len, 0, 1);

	Com_Printf("send TO MAC: %02X:%02X:%02X:%02X:%02X:%02X \n",
	   ((sockaddr_adhoc *)addr)->mac[0],
	   ((sockaddr_adhoc *)addr)->mac[1],
	   ((sockaddr_adhoc *)addr)->mac[2],
	   ((sockaddr_adhoc *)addr)->mac[3],
	   ((sockaddr_adhoc *)addr)->mac[4],
	   ((sockaddr_adhoc *)addr)->mac[5]);
	   
	if(ret < 0)
	{
	   Com_Printf("WLAN: error send 0x%08X MAC: %02X:%02X:%02X:%02X:%02X:%02X \n", ret,
	   ((sockaddr_adhoc *)addr)->mac[0],
	   ((sockaddr_adhoc *)addr)->mac[1],
	   ((sockaddr_adhoc *)addr)->mac[2],
	   ((sockaddr_adhoc *)addr)->mac[3],
	   ((sockaddr_adhoc *)addr)->mac[4],
	   ((sockaddr_adhoc *)addr)->mac[5]);
	}
	return ret;
}

char* Adhoc_AddrToString (struct sockaddr *addr)
{
	static char buffer[22];

	sceNetEtherNtostr((unsigned char *)((sockaddr_adhoc *)addr)->mac, buffer);
	sprintf(buffer + strlen(buffer), ":%d", ((sockaddr_adhoc *)addr)->port);
	return buffer;
}

int Adhoc_StringToAddr(char *string, struct sockaddr *addr)
{
	int ha1, ha2, ha3, ha4, ha5, ha6, hp;

	sscanf(string, "%x:%x:%x:%x:%x:%x:%d", &ha1, &ha2, &ha3, &ha4, &ha5, &ha6, &hp);
	addr->sa_family = ADHOC_NET;
	((struct sockaddr_adhoc *)addr)->mac[0] = ha1 & 0xFF;
	((struct sockaddr_adhoc *)addr)->mac[1] = ha2 & 0xFF;
	((struct sockaddr_adhoc *)addr)->mac[2] = ha3 & 0xFF;
	((struct sockaddr_adhoc *)addr)->mac[3] = ha4 & 0xFF;
	((struct sockaddr_adhoc *)addr)->mac[4] = ha5 & 0xFF;
	((struct sockaddr_adhoc *)addr)->mac[5] = ha6 & 0xFF;
	((struct sockaddr_adhoc *)addr)->port = hp & 0xFFFF;

	return 1;
}

int Adhoc_AddrCompare(struct sockaddr *addr1, struct sockaddr *addr2)
{

	if (memcmp(((struct sockaddr_adhoc *)addr1)->mac, ((struct sockaddr_adhoc *)addr2)->mac, 6) != 0)
	    return -1;

	if (((struct sockaddr_adhoc *)addr1)->port != ((struct sockaddr_adhoc *)addr2)->port)
		return 1;

	return 0;
}

int Adhoc_Init(void)
{
	int rc;

    if(!sceWlanGetSwitchState())
    {
	   Com_Printf("wlan button is disabled\n");
	   return 0;
    }

	if(!libReady)
	{
       sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	   sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
    }

	rc = pspSdkAdhocInit("ULUS000767");
	if(rc < 0)
	{
		Com_Printf("AdHocINIT: failed: 0X%08X\n", rc);
		Adhoc_Term ();
		Adhoc_Shutdown ();
		libReady = 0;
		return 0;
	}

	rc = sceNetAdhocctlConnect("Quake2");
	if(rc < 0)
	{
		Com_Printf("AdHocINIT: AdhocctlConnect failed: 0X%08X\n", rc);
		Adhoc_Term ();
		Adhoc_Shutdown ();
		libReady = 0;
		return 0;
	}

    int stateLast = -1;
	while (1)
	{
		int state;
		rc = sceNetAdhocctlGetState(&state);
		if (rc != 0)
		{
			Com_Printf("Couldn't initialise the network %08X\n", rc);
			Adhoc_Term ();
			Adhoc_Shutdown ();
			return 0;
		}
		if (state > stateLast)
		{
			stateLast = state;
		}
		if (state == 1)
		{
			break;
		}
		sceKernelDelayThread(50*1000); // 50ms
	}
				
	Com_Printf("AdHoc: Initialized\n");
	return 1;
}

void Adhoc_Term (void)
{
    pspSdkAdhocTerm();
}

void Adhoc_Shutdown (void)
{
	if(libReady)
	{
	  sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
 	  sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    }
	libReady = 0;
}



