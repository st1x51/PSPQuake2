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
// net_wins.c

#include "../qcommon/qcommon.h"

#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include <sys/socket.h>
#include <psptypes.h>
#include <pspwlan.h>
#include <pspsdk.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include "gethost.hpp"

//#define SCESOCKET

cvar_t* net_wificonnection;

netadr_t net_local_adr;

#define	LOOPBACK		0x7f000001
#define	MAX_LOOPBACK	4

typedef struct {
	byte	data[MAX_MSGLEN];
	int		datalen;
} loopmsg_t;

typedef struct {
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

loopback_t	loopbacks[2];
int			ip_sockets[]={-1,-1};

int NET_Socket(char *net_interface, int port);
char* NET_ErrorString(void);

int netConnected = 0;

//=============================================================================

void NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
	memset (s, 0, sizeof(*s));

	if (a->type == NA_BROADCAST)
	{
		s->sin_family = AF_INET;

		s->sin_port = a->port;
		*(int *)&s->sin_addr = -1;
	}
	else if (a->type == NA_IP)
	{
		s->sin_family = AF_INET;
		*(int *)&s->sin_addr = *(int *)&a->ip;
		s->sin_port = a->port;
	}
}

void SockadrToNetadr (struct sockaddr_in *s, netadr_t *a)
{
    *(int *)&a->ip = *(int *)&s->sin_addr;
    a->port = s->sin_port;
    a->type = NA_IP;
}


qboolean	NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
		return true;
	return false;
}

/*
===================
NET_CompareBaseAdr

Compares without the port
===================
*/
qboolean	NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (a.type != b.type)
		return false;

	if (a.type == NA_LOOPBACK)
		return true;

	if (a.type == NA_IP)
	{
		if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
			return true;
		return false;
	}
/*
	if (a.type == NA_IPX)
	{
		if ((memcmp(a.ipx, b.ipx, 10) == 0))
			return true;
		return false;
	}
*/
}

char	*NET_AdrToString (netadr_t a)
{
 	static	char	s[64];
	Com_sprintf (s, sizeof(s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
	return s;
}

char	*NET_BaseAdrToString (netadr_t a)
{
	static	char	s[64];
	Com_sprintf (s, sizeof(s), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
	return s;
}


/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qboolean NET_StringToSockaddr(char *s, struct sockaddr *sadr)
{
    struct hostent	*h;
	char	*colon;
	char	copy[128];

    memset (sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;

	((struct sockaddr_in *)sadr)->sin_port = 0;

	strcpy (copy, s);
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));
		}

    if (copy[0] >= '0' && copy[0] <= '9')
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = sceNetInetInetAddr(copy);
	}
	else
	{
		if (! (h = gethostbyname(copy)) )
			return 0;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return true;
}

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
qboolean	NET_StringToAdr (char *s, netadr_t *a)
{
	struct sockaddr_in sadr;

	if (!strcmp (s, "localhost"))
	{
		memset (a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	if (!NET_StringToSockaddr (s, (struct sockaddr *)&sadr))
		return false;

	SockadrToNetadr (&sadr, a);

	return true;
}

qboolean	NET_IsLocalAddress (netadr_t adr)
{
	return NET_CompareAdr (adr, net_local_adr);
}

/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/


qboolean	NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	memcpy (net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	*net_from = net_local_adr;
	return true;

}


void NET_SendLoopPacket (netsrc_t sock, int length, void *data, netadr_t to)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

//=============================================================================

qboolean NET_GetPacket(netsrc_t sock, netadr_t* net_from, sizebuf_t* net_message) 
{
	if(NET_GetLoopPacket(sock, net_from, net_message)) 
	{
		return true;
	}

	int net_socket = ip_sockets[sock];
	if(net_socket == -1)
	{
		return false;
	}

	struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int ret;
#ifdef SCESOCKET
	ret = sceNetInetRecvfrom(net_socket, net_message->data, net_message->maxsize, 0, (struct sockaddr*)&from, &fromlen);
#else
	ret = recvfrom(net_socket, net_message->data, net_message->maxsize, 0, (struct sockaddr*)&from, &fromlen);
#endif //SCESOCKET

	if(ret == -1)
	{
		int err = errno;
		if(err == EWOULDBLOCK || err == ECONNREFUSED)
		{
			return false;
		}

		Com_Printf("NET_GetPacket: %s from %s\n", NET_ErrorString(), NET_AdrToString(*net_from));
		return false;
    }

    SockadrToNetadr(&from, net_from);


    net_message->cursize = ret;

	return true;
}

//=============================================================================

void NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to) 
{
	if(to.type == NA_LOOPBACK) 
	{
		NET_SendLoopPacket (sock, length, data, to);
		return;
	}

	int net_socket = ip_sockets[sock];
	if(net_socket == -1)
	{
		return;
	}

	struct sockaddr_in	addr;

	NetadrToSockadr(&to, &addr);
#ifdef SCESOCKET
	if(sceNetInetSendto(net_socket, data, length, 0, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		Com_Printf("NET_SendPacket ERROR: %s to %s\n", NET_ErrorString(), NET_AdrToString (to));
	}
#else
	if(sendto(net_socket, data, length, 0, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		Com_Printf("NET_SendPacket ERROR: %s to %s\n", NET_ErrorString(), NET_AdrToString (to));
	}
#endif
}

//=============================================================================


/*
====================
NET_OpenIP
====================
*/
void NET_OpenIP(void)
{
	cvar_t* ip = Cvar_Get("ip", "localhost", CVAR_NOSET);
    cvar_t* port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET);

	if(!ip_sockets[NS_SERVER])
	{
		ip_sockets[NS_SERVER] = NET_Socket(ip->string, port->value);
	}
	if(!ip_sockets[NS_CLIENT])
	{
		ip_sockets[NS_CLIENT] = NET_Socket(ip->string, PORT_ANY);
	}
}

/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/
void S_StopAllSounds();

void NET_Config(qboolean multiplayer) 
{
	S_StopAllSounds();

	if(!multiplayer)
	{
		int i;
		for(i = 0; i < 2; i++) 
		{
				if(ip_sockets[i] != -1)
				{
					{
#ifdef SCESOCKET
						sceNetInetClose(ip_sockets[i]);
#else
						close(ip_sockets[i]);
#endif
					}
					ip_sockets[i] = -1;
				}
		}
		netConnected = 0;
	}
	else
	{
		if(net_wificonnection->modified)
		{
				int i;
				for(i = 0; i < 2; i++)
				{
					if(ip_sockets[i])
					{
#ifdef SCESOCKET
						sceNetInetClose(ip_sockets[i]);
#else
						close(ip_sockets[i]);
#endif
						ip_sockets[i] = 0;
					}
				}
				netConnected = 0;

				int rc = sceNetApctlDisconnect();

				if(net_wificonnection->value && sceWlanGetSwitchState())
				{
					rc = sceNetApctlConnect((int)net_wificonnection->value);
					if(rc != 0)
					{
						Com_Printf("WiFi: Error sceNetApctlConnect(%d)", (int)net_wificonnection->value);
						return;
					}

					int state = 0;
					int oldstate = 0;
	                int timeout = 0;

					do {
						oldstate = state;
						rc = sceNetApctlGetState(&state);
						if(rc != 0)
						{
							Com_Printf("WiFi: Error sceNetApctlGetState()\n");
							sceNetApctlDisconnect();
							return;
						}

						if(state == 4)
						{
							net_wificonnection->modified = 0;
							break;
						}

						if(oldstate > state)
						{
							Com_Printf("WiFi: Error oldstate > state\n");
							sceNetApctlDisconnect();
							return;
						}

						if(timeout > 120)
						{
							Com_Printf("WiFi: Error timeout\n");
							sceNetApctlDisconnect();
							return;
						}

						printf("state:%d\n", state);

						sceKernelDelayThread(250000);
						timeout++;
					} while(state != 4);
					netConnected = 1;
				}
        }
        NET_OpenIP();
	}
}


//===================================================================


/*
====================
NET_Init
====================
*/
void NET_Init(void) 
{
    net_wificonnection = Cvar_Get("net_wificonnection", "0", CVAR_ARCHIVE);

	// Load the network modules when they are required
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

	// Initialise the network.
	const int err = pspSdkInetInit();
	if (err)
	{
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
		sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
		Sys_Error("Couldn't initialise the network %08X\n", err);
	}

}


/*
====================
NET_Socket
====================
*/
int NET_Socket(char* net_interface, int port)
{
	int newsocket;
	int	i = 1;
#ifdef SCESOCKET
	if((newsocket = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		Com_Printf("Error: sceNetInetSocket: %s\n", NET_ErrorString());
		return 0;
	}
#else
	if((newsocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		Com_Printf("Error: sceNetInetSocket: %s\n", NET_ErrorString());
		return 0;
	}
#endif

#ifdef SCESOCKET
	if(sceNetInetSetsockopt(newsocket, SOL_SOCKET, SO_NONBLOCK, &i, sizeof(i)) == -1)
	{
		sceNetInetClose(newsocket);
		Com_Printf("Error: sceNetInetSetsockopt SO_NONBLOCK: %s\n", NET_ErrorString());
		return 0;
	}
#else
	if(setsockopt(newsocket, SOL_SOCKET, SO_NONBLOCK, &i, sizeof(i)) == -1)
	{
		sceNetInetClose(newsocket);
		Com_Printf("Error: sceNetInetSetsockopt SO_NONBLOCK: %s\n", NET_ErrorString());
		return 0;
	}
#endif

#ifdef SCESOCKET
	if(sceNetInetSetsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i)) == -1) {
		sceNetInetClose(newsocket);
		Com_Printf("Error: sceNetInetSetsockopt SO_BROADCAST: %s\n", NET_ErrorString());
		return 0;
	}
#else
	if(setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i)) == -1) {
		sceNetInetClose(newsocket);
		Com_Printf("Error: sceNetInetSetsockopt SO_BROADCAST: %s\n", NET_ErrorString());
		return 0;
	}
#endif
	struct sockaddr_in address;
	if(!net_interface || !net_interface[0] || !stricmp(net_interface, "localhost"))
	{
		memset(&address, 0, sizeof(address));
		address.sin_addr.s_addr = 0;
	}
	else
	{
		NET_StringToSockaddr(net_interface, (struct sockaddr *)&address);
	}

	if(port == PORT_ANY)
	{
		address.sin_port = 0;
	}
	else
	{
		address.sin_port = htons((short)port);
	}
	address.sin_family = AF_INET;

#ifdef SCESOCKET
	if(sceNetInetBind(newsocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
		sceNetInetClose(newsocket);
		Com_Printf("Error: sceNetInetBind: %s\n", NET_ErrorString());
		return 0;
	}
#else
	if(bind(newsocket, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		close(newsocket);
		Com_Printf("Error: bind: %s\n", NET_ErrorString());
		return 0;
	}
#endif
	return newsocket;
}

/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown(void) 
{
	NET_Config(false);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

/*
====================
NET_ErrorString
====================
*/
char* NET_ErrorString(void)
{
	int	code;
	code = errno;
	return strerror(code);
}

// sleeps msec or until net socket is ready
void NET_Sleep(int msec)
{
#if 0 // PSP_REMOVE always run full speed
    struct timeval timeout;
	fd_set	fdset;
	extern cvar_t *dedicated;
	//extern qboolean stdin_active;

	if (!ip_sockets[NS_SERVER] || (dedicated && !dedicated->value))
		return; // we're not a server, just run full speed

	FD_ZERO(&fdset);
//	if (stdin_active)
//		FD_SET(0, &fdset); // stdin is processed too
	FD_SET(ip_sockets[NS_SERVER], &fdset); // network socket
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(ip_sockets[NS_SERVER]+1, &fdset, NULL, NULL, &timeout);
#endif
}
