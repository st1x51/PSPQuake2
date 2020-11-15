/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Peter Mackay and Chris Swindle.

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

//#include <psputility_netmodules.h>
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspwlan.h>
#include <pspnet_apctl.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>

#include "netfunc_psp.hpp"

#include <arpa/inet.h>	// inet_addr
#include <netinet/in.h>	// sockaddr_in
#include <sys/socket.h>	// PSP socket API.
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>

#include <gethost.hpp>

#include <pspsdk.h>

#define ADHOC_NET 29
#define ADHOC_EWOULDBLOCK	0x80410709

#define MAXHOSTNAMELEN	64
#define EWOULDBLOCK		111
#define ECONNREFUSED	11

typedef struct sockaddr_adhoc
{
    char   len;
    short family;
    u16	port;
    char mac[6];
	char zero[6];
} sockaddr_adhoc;

static pdpStatStruct gPdpStat;
pdpStatStruct *findPdpStat(int socket, pdpStatStruct *pdpStat)
{
	if(socket == pdpStat->pdpId)
	{
		memcpy(&gPdpStat, pdpStat, sizeof(pdpStatStruct));
		return &gPdpStat;
	}
		if(pdpStat->next) return findPdpStat(socket, pdpStat->next);
		return (pdpStatStruct *)-1;
}

static int accept_socket = -1;		// socket for fielding new connections
static int control_socket = -1;
static int broadcast_socket = 0;
static struct sockaddr broadcast_addr;
static int my_addr = 0;

int init (void)
{
	struct sockaddr addr;
	char *colon;
	char	buff[MAXHOSTNAMELEN];
	int i,rc;

	S_ClearBuffer ();

	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);

	int stateLast = -1;
	rc = pspSdkAdhocInit("ULUS00443");
	if(rc < 0)
	{
		Com_Printf("Couldn't initialise the network %08X\n", rc);
		sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
		sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
		return -1;
	}

	rc = sceNetAdhocctlConnect((char *)"QuakeII");
	if(rc < 0)
	{
		Com_Printf("Couldn't initialise the network %08X\n", rc);
		pspSdkAdhocTerm();
		sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
		sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
		return -1;
	}

	while (1)
	{
		int state;
		rc = sceNetAdhocctlGetState(&state);
		if (rc != 0)
		{
			Com_Printf("Couldn't initialise the network %08X\n", rc);
			pspSdkAdhocTerm();
			sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
			sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
			return -1;
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

	gethostname(buff, MAXHOSTNAMELEN);
	if (Q_strcmp(hostname.string, "UNNAMED") == 0)
	{
		buff[15] = 0;
		Cvar_Set ("hostname", buff);
	}

	if ((control_socket = open_socket(0)) == -1)
	{
		pspSdkAdhocTerm();
		sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
		sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
		Sys_Error("init: Unable to open control socket\n");
	}

	((sockaddr_adhoc *)&broadcast_addr)->family = ADHOC_NET;
	for(i=0; i<6; i++) ((sockaddr_adhoc *)&broadcast_addr)->mac[i] = 0xFF;
	((sockaddr_adhoc *)&broadcast_addr)->port = net_hostport;

	get_socket_addr (control_socket, &addr);
	Q_strcpy(my_tcpip_address,  addr_to_string(&addr));
	colon = Q_strrchr(my_tcpip_address, ':');
	if (colon) *colon = 0;

	listen(qtrue);

	Com_Printf("AdHoc Initialized\n");
	
	return control_socket;
}

//=============================================================================

void shut_down (void)
{
	listen(qfalse);
	close_socket(control_socket);
	pspSdkAdhocTerm();

	// Now to unload the network modules, no need to keep them loaded all the time
	sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

//=============================================================================

void listen (qboolean state)
{
	// enable listening
	if (state)
	{
		if (accept_socket != -1)
			return;
		if ((accept_socket = open_socket(net_hostport)) == -1)
			Sys_Error ("listen: Unable to open accept socket\n");
		return;
	}

	// disable listening
	if (accept_socket == -1)
		return;
	close_socket(accept_socket);
	accept_socket = -1;
}

//=============================================================================

int open_socket (int port)
{
	u8 mac[8];
	sceWlanGetEtherAddr(mac);
	int rc = sceNetAdhocPdpCreate(mac, port, 0x2000, 0);
	if(rc < 0) return -1;
	return rc;
}

//=============================================================================

int close_socket (int socket)
{
	if (socket == broadcast_socket) broadcast_socket = 0;
	return sceNetAdhocPdpDelete(socket, 0);
}

int connect (int socket, struct sockaddr *addr)
{
	return 0;
}

//=============================================================================

int check_new_connections (void)
{
	pdpStatStruct pdpStat[20];
	int length = sizeof(pdpStatStruct) * 20;

	if (accept_socket == -1)
		return -1;

	int err = sceNetAdhocGetPdpStat(&length, pdpStat);
	if(err < 0) return -1;

	pdpStatStruct *tempPdp = findPdpStat(accept_socket, pdpStat);

	if(tempPdp < 0) return -1;

	if(tempPdp->rcvdData > 0) return accept_socket;

	return -1;
}

//=============================================================================

int read (int socket, byte *buf, int len, struct sockaddr *addr)
{
	unsigned short port;
	int datalength = len;
	unsigned int ret;

	sceKernelDelayThread(1);

	ret = sceNetAdhocPdpRecv(socket, (unsigned char *)((sockaddr_adhoc *)addr)->mac, &port, buf, &datalength, 0, 1);
	if(ret == ADHOC_EWOULDBLOCK) return 0;

	((sockaddr_adhoc *)addr)->port = port;

	return datalength;
}

//=============================================================================

static int make_socket_broadcast_capable (int socket)
{
	broadcast_socket = socket;
	return 0;
}

//=============================================================================

int broadcast (int socket, byte *buf, int len)
{
	int ret;

	if (socket != broadcast_socket)
	{
		if (broadcast_socket != 0)
			Sys_Error("Attempted to use multiple broadcasts sockets\n");
		ret = make_socket_broadcast_capable (socket);
		if (ret == -1)
		{
			Com_Printf("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return write(socket, buf, len, &broadcast_addr);
}

//=============================================================================

int write (int socket, byte *buf, int len, struct sockaddr *addr)
{
	int ret = -1;

	ret = sceNetAdhocPdpSend(socket, (unsigned char*)((sockaddr_adhoc *)addr)->mac, ((sockaddr_adhoc *)addr)->port, buf, len, 0, 1);

	if(ret < 0) Com_Printf("Failed to send message, errno=%08X\n", ret);
	return ret;
}

//=============================================================================

char* addr_to_string (struct sockaddr *addr)
{
	static char buffer[22];

	sceNetEtherNtostr((unsigned char *)((sockaddr_adhoc *)addr)->mac, buffer);
	sprintf(buffer + strlen(buffer), ":%d", ((sockaddr_adhoc *)addr)->port);
	return buffer;
}

//=============================================================================

int string_to_addr (char *string, struct sockaddr *addr)
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
	return 0;
}

//=============================================================================

int get_socket_addr (int socket, struct sockaddr *addr)
{
	pdpStatStruct pdpStat[20];
	int length = sizeof(pdpStatStruct) * 20;

	int err = sceNetAdhocGetPdpStat(&length, pdpStat);
	if(err<0) return -1;

	pdpStatStruct *tempPdp = findPdpStat(socket, pdpStat);
	if(tempPdp < 0) return -1;

	memcpy(((struct sockaddr_adhoc *)addr)->mac, tempPdp->mac, 6);
	((struct sockaddr_adhoc *)addr)->port = tempPdp->port;
	addr->sa_family = ADHOC_NET;
	return 0;
}

//=============================================================================

int get_name_from_addr (struct sockaddr *addr, char *name)
{
	strcpy(name, addr_to_string(addr));
	return 0;
}

//=============================================================================

int get_addr_from_name(char *name, struct sockaddr *addr)
{
	return string_to_addr(name, addr);
}

//=============================================================================

int addr_compare (struct sockaddr *addr1, struct sockaddr *addr2)
{
	//if (addr1->sa_family != addr2->sa_family) return -1;
	if (memcmp(((struct sockaddr_adhoc *)addr1)->mac, ((struct sockaddr_adhoc *)addr2)->mac, 6) != 0) return -1;
	if (((struct sockaddr_adhoc *)addr1)->port != ((struct sockaddr_adhoc *)addr2)->port) return 1;
	return 0;
}

//=============================================================================

int get_socket_port (struct sockaddr *addr)
{
	return ((struct sockaddr_adhoc *)addr)->port;
}


int set_socket_port (struct sockaddr *addr, int port)
{
	((struct sockaddr_adhoc *)addr)->port = port;
	return 0;
}

//=============================================================================

int pspSdkAdhocInit(char *product)
{
	u32 retVal;
	struct productStruct temp;

	retVal = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
	if (retVal != 0) return retVal;

	retVal = sceNetAdhocInit();
	if (retVal != 0) return retVal;

	strcpy(temp.product, product);
	temp.unknown = 0;

	retVal = sceNetAdhocctlInit(0x2000, 0x20, &temp);
	if (retVal != 0) return retVal;

	return 0;
}

//=============================================================================

void pspSdkAdhocTerm()
{
	sceNetAdhocctlTerm();
	sceNetAdhocTerm();
	sceNetTerm();
}

//=============================================================================

