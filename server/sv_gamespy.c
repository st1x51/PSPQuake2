/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.
*/

/*
 * Kingpin's copy of the GameSpy Open SDK (kingpin.exe
 * 00424bc0..004259c5).  This is deliberately kept separate from the
 * Quake II connectionless/master protocol in sv_main.c: the retail game
 * opens a second UDP query socket on game_port - 10 and sends an
 * additional GameSpy heartbeat to master.gamespy.com:27900.
 */

#if defined(KINGPIN_ENGINE) && defined(_WIN32)
#include "winsock.h"
#elif defined(KINGPIN_ENGINE)
#include <stdint.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#ifdef __psp__
#include <pspnet_inet.h>
#define socket sceNetInetSocket
#define bind sceNetInetBind
#define connect sceNetInetConnect
#define send sceNetInetSend
#define sendto sceNetInetSendto
#define recvfrom sceNetInetRecvfrom
#define select sceNetInetSelect
#define closesocket sceNetInetClose
#else
#define closesocket close
#endif
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef uint32_t DWORD;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffffU
#endif
#endif
#include "server.h"

#ifdef KINGPIN_ENGINE

#ifndef _WIN32
#define GetTickCount() ((DWORD)Sys_Milliseconds())
#endif

#define GAMESPY_MASTER_HOST        "master.gamespy.com"
#define GAMESPY_MASTER_PORT        27900
#define GAMESPY_PACKET_SIZE        1400
#define GAMESPY_APPEND_LIMIT       1349
#define GAMESPY_HEARTBEAT_MSEC     300000

static SOCKET gs_query_socket = INVALID_SOCKET;
static SOCKET gs_heartbeat_socket = INVALID_SOCKET;
#ifdef _WIN32
static qboolean gs_wsa_started;
#endif
static int gs_query_port;
/* The embedded SDK starts at one, then increments before every query. */
static int gs_query_id = 1;
static int gs_packet_id;
static DWORD gs_last_heartbeat;
/*
kingpin.exe @0043d694 installs the basic/info/rules/players callbacks and
the "QFWxY2" secret key only when goa_init succeeds. When it fails (e.g. the
master host does not resolve) the query socket stays open and answers
every query with just \final\ (and the echo), as the retail server does.
*/
static qboolean gs_callbacks;

static const char *GS_ValueForKey (const char *query, const char *key,
	char *value, size_t value_size)
{
	const char *p;
	size_t key_length;
	size_t length;

	if (!query || !key || !value || !value_size)
		return NULL;

	key_length = strlen (key);
	p = query;
	while ((p = strchr (p, '\\')) != NULL)
	{
		p++;
		if (!strncmp (p, key, key_length) &&
			(p[key_length] == '\\' || p[key_length] == 0))
		{
			p += key_length;
			if (*p == '\\')
				p++;
			length = strcspn (p, "\\");
			if (length >= value_size)
				length = value_size - 1;
			memcpy (value, p, length);
			value[length] = 0;
			return value;
		}
	}

	return NULL;
}

static void GS_SendPacket (const struct sockaddr_in *to, char *response)
{
	char queryid[80];

	if (!response[0])
		return;

	gs_packet_id++;
	Com_sprintf (queryid, sizeof(queryid), "\\queryid\\%d.%d",
		gs_query_id, gs_packet_id);
	strncat (response, queryid,
		GAMESPY_PACKET_SIZE - strlen(response) - 1);

	sendto (gs_query_socket, response, (int)strlen(response), 0,
		(const struct sockaddr *)to, sizeof(*to));
	response[0] = 0;
}

static void GS_Append (const struct sockaddr_in *to, char *response,
	const char *text)
{
	if (!text || !text[0])
		return;

	if (strlen(response) + strlen(text) > GAMESPY_APPEND_LIMIT)
		GS_SendPacket (to, response);

	strncat (response, text,
		GAMESPY_PACKET_SIZE - strlen(response) - 1);
}

static void GS_AppendBasic (const struct sockaddr_in *to, char *response)
{
	char text[256];
	cvar_t *location;

	if (!gs_callbacks)
		return;

	location = Cvar_Get ("location", "", 0);
	Com_sprintf (text, sizeof(text),
		"\\gamename\\kingpin\\gamever\\%3.2f\\location\\%s",
		VERSION, location->string);
	GS_Append (to, response, text);
}

static int GS_PlayerCount (void)
{
	int i;
	int count;

	count = 0;
	for (i = 0; i < (int)maxclients->value; i++)
		if (svs.clients[i].state == cs_connected ||
			svs.clients[i].state == cs_spawned)
			count++;

	return count;
}

static void GS_AppendInfo (const struct sockaddr_in *to, char *response)
{
	char text[512];
	cvar_t *port;
	cvar_t *gametype;

	if (!gs_callbacks)
		return;

	port = Cvar_Get ("port", va("%i", PORT_SERVER), CVAR_NOSET);
	gametype = Cvar_Get ("gamename", "", 0);
	Com_sprintf (text, sizeof(text),
		"\\hostname\\%s\\hostport\\%d\\mapname\\%s"
		"\\gametype\\%s\\numplayers\\%d\\maxplayers\\%d"
		"\\gamemode\\openplaying",
		hostname->string, atoi(port->string), sv.name, gametype->string,
		GS_PlayerCount(), (int)maxclients->value);
	GS_Append (to, response, text);
}

static void GS_AppendRules (const struct sockaddr_in *to, char *response)
{
	if (!gs_callbacks)
		return;
	GS_Append (to, response, Cvar_Serverinfo ());
}

static void GS_AppendPlayers (const struct sockaddr_in *to, char *response)
{
	char text[192];
	client_t *cl;
	int i;
	int player;

	if (!gs_callbacks)
		return;

	player = 0;
	for (i = 0; i < (int)maxclients->value; i++)
	{
		cl = &svs.clients[i];
		if (cl->state != cs_connected && cl->state != cs_spawned)
			continue;

		Com_sprintf (text, sizeof(text),
			"\\player_%d\\%s\\frags_%d\\%d\\ping_%d\\%d",
			player, cl->name, player,
			cl->edict->client->ps.stats[STAT_FRAGS], player, cl->ping);
		GS_Append (to, response, text);
		player++;
	}
}

/* kingpin.exe 004250a0: the GameSpy challenge cipher. */
static void GS_EncryptChallenge (const char *key, byte *data, int length)
{
	byte state[256];
	unsigned int keypos;
	unsigned int i;
	unsigned int j;
	byte swap;

	for (i = 0; i < 256; i++)
		state[i] = (byte)i;

	keypos = 0;
	j = 0;
	for (i = 0; i < 256; i++)
	{
		j = (byte)(state[i] + j + (byte)key[keypos]);
		keypos = (keypos + 1) % strlen(key);
		swap = state[i];
		state[i] = state[j];
		state[j] = swap;
	}

	i = 0;
	j = 0;
	for (keypos = 0; keypos < (unsigned int)length; keypos++)
	{
		i = (byte)(data[keypos] + 1 + i);
		j = (byte)(state[i] + j);
		swap = state[i];
		state[i] = state[j];
		state[j] = swap;
		data[keypos] ^= state[(byte)(state[j] + state[i])];
	}
}

/* kingpin.exe 00424fc0/00424ff0: standard GameSpy base-64 alphabet. */
static void GS_EncodeChallenge (const byte *input, int length, char *output,
	size_t output_size)
{
	static const char alphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned int value;
	int i;
	size_t out;

	out = 0;
	for (i = 0; i < length && out + 4 < output_size; i += 3)
	{
		value = input[i] << 16;
		if (i + 1 < length)
			value |= input[i + 1] << 8;
		if (i + 2 < length)
			value |= input[i + 2];
		output[out++] = alphabet[(value >> 18) & 63];
		output[out++] = alphabet[(value >> 12) & 63];
		output[out++] = alphabet[(value >> 6) & 63];
		output[out++] = alphabet[value & 63];
	}
	output[out] = 0;
}

static void GS_AppendFinal (const struct sockaddr_in *to, char *response,
	const char *challenge)
{
	byte encrypted[129];
	char encoded[200];
	char text[256];
	int length;

	// without the key kingpin.exe would divide by zero here; skip instead
	if (gs_callbacks && challenge && challenge[0])
	{
		length = (int)strlen(challenge);
		if (length <= 128)
		{
			memcpy (encrypted, challenge, length);
			GS_EncryptChallenge ("QFWxY2", encrypted, length);
			GS_EncodeChallenge (encrypted, length, encoded, sizeof(encoded));
			Com_sprintf (text, sizeof(text), "\\validate\\%s", encoded);
			GS_Append (to, response, text);
		}
	}

	GS_Append (to, response, "\\final\\");
	GS_SendPacket (to, response);
}

static void GS_ProcessQuery (const char *query, const struct sockaddr_in *to)
{
	char response[GAMESPY_PACKET_SIZE];
	char value[256];
	char challenge[256];

	response[0] = 0;
	challenge[0] = 0;
	gs_query_id++;
	gs_packet_id = 0;

	if (GS_ValueForKey(query, "basic", value, sizeof(value)))
		GS_AppendBasic (to, response);
	if (GS_ValueForKey(query, "info", value, sizeof(value)))
		GS_AppendInfo (to, response);
	if (GS_ValueForKey(query, "rules", value, sizeof(value)))
		GS_AppendRules (to, response);
	if (GS_ValueForKey(query, "players", value, sizeof(value)))
		GS_AppendPlayers (to, response);
	if (GS_ValueForKey(query, "status", value, sizeof(value)))
	{
		GS_AppendBasic (to, response);
		GS_AppendInfo (to, response);
		GS_AppendRules (to, response);
		GS_AppendPlayers (to, response);
	}
	if (GS_ValueForKey(query, "packets", value, sizeof(value)))
	{
		GS_AppendBasic (to, response);
		GS_SendPacket (to, response);
		GS_AppendInfo (to, response);
		GS_SendPacket (to, response);
		GS_AppendRules (to, response);
		GS_SendPacket (to, response);
		GS_AppendPlayers (to, response);
	}
	if (GS_ValueForKey(query, "echo", value, sizeof(value)) &&
		strlen(value) < GAMESPY_APPEND_LIMIT)
	{
		char echo[320];
		Com_sprintf (echo, sizeof(echo), "\\echo\\%s", value);
		GS_Append (to, response, echo);
	}
	if (GS_ValueForKey(query, "secure", challenge, sizeof(challenge)))
	{
		/* value retained for GS_AppendFinal */
	}

	GS_AppendFinal (to, response, challenge);
}

static void GS_SendHeartbeat (qboolean state_changed)
{
	char heartbeat[256];

	if (gs_heartbeat_socket == INVALID_SOCKET)
		return;

	Com_sprintf (heartbeat, sizeof(heartbeat),
		"\\heartbeat\\%d\\gamename\\kingpin", gs_query_port);
	if (state_changed)
		strcat (heartbeat, "\\statechanged\\");
	send (gs_heartbeat_socket, heartbeat, (int)strlen(heartbeat), 0);
	gs_last_heartbeat = GetTickCount ();
}

int SV_GameSpyInit (void)
{
#ifdef _WIN32
	WSADATA wsa;
#endif
	SOCKADDR_IN address;
#ifndef __psp__
	struct hostent *host;
#endif
	cvar_t *port;
	cvar_t *ip;
#ifdef _WIN32
	int result;
#endif

	if (!dedicated || !dedicated->value)
		return 0;
	SV_GameSpyShutdown ();
	gs_callbacks = false;

	port = Cvar_Get ("port", va("%i", PORT_SERVER), CVAR_NOSET);
	ip = Cvar_Get ("ip", "localhost", CVAR_NOSET);
	gs_query_port = atoi(port->string) - 10;

#ifdef _WIN32
	result = WSAStartup (MAKEWORD(1, 1), &wsa);
	if (result)
		return 1;
	gs_wsa_started = true;
#endif

	gs_query_socket = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	gs_heartbeat_socket = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (gs_query_socket == INVALID_SOCKET ||
		gs_heartbeat_socket == INVALID_SOCKET)
		return 1;

	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons ((u_short)gs_query_port);
	if (ip->string[0] && Q_stricmp(ip->string, "localhost"))
	{
		address.sin_addr.s_addr = inet_addr (ip->string);
		if (address.sin_addr.s_addr == INADDR_NONE)
		{
#ifdef __psp__
			netadr_t resolved;
			if (!NET_StringToAdr(ip->string, &resolved, 0) || resolved.type != NA_IP)
				return 3;
			memcpy (&address.sin_addr.s_addr, resolved.ip, 4);
#else
			host = gethostbyname (ip->string);
			if (!host)
				return 3;
			memcpy (&address.sin_addr.s_addr, host->h_addr_list[0], 4);
#endif
		}
		if (address.sin_addr.s_addr == inet_addr("127.0.0.1"))
			address.sin_addr.s_addr = INADDR_ANY;
	}
	else
	{
		address.sin_addr.s_addr = INADDR_ANY;
	}

	if (bind(gs_query_socket, (struct sockaddr *)&address,
		sizeof(address)) == SOCKET_ERROR)
		return 2;

	address.sin_port = htons (GAMESPY_MASTER_PORT);
#ifdef __psp__
	{
		netadr_t resolved;
		if (!NET_StringToAdr(GAMESPY_MASTER_HOST, &resolved, 0) || resolved.type != NA_IP)
			return 3;
		memcpy (&address.sin_addr.s_addr, resolved.ip, 4);
	}
#else
	host = gethostbyname (GAMESPY_MASTER_HOST);
	if (!host)
		return 3;
	memcpy (&address.sin_addr.s_addr, host->h_addr_list[0], 4);
#endif
	if (connect(gs_heartbeat_socket, (struct sockaddr *)&address,
		sizeof(address)) == SOCKET_ERROR)
		return 4;

	gs_callbacks = true;
	return 0;
}

void SV_GameSpyFrame (void)
{
	SOCKADDR_IN from;
#ifdef _WIN32
	int from_length;
#else
	socklen_t from_length;
#endif
	char query[256];
	int length;
	fd_set read_set;
#ifdef __psp__
	struct SceNetInetTimeval timeout;
#else
	struct timeval timeout;
#endif
	DWORD now;

	if (!dedicated || !dedicated->value ||
		!public_server || !public_server->value ||
		gs_query_socket == INVALID_SOCKET ||
		gs_heartbeat_socket == INVALID_SOCKET)
		return;

	now = GetTickCount ();
	if ((DWORD)(now - gs_last_heartbeat) > GAMESPY_HEARTBEAT_MSEC)
		GS_SendHeartbeat (false);

	for (;;)
	{
		FD_ZERO (&read_set);
		FD_SET (gs_query_socket, &read_set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		length = select (gs_query_socket + 1, &read_set, NULL, NULL, &timeout);
		if (length <= 0)
			break;

		from_length = sizeof(from);
		length = recvfrom (gs_query_socket, query, sizeof(query) - 1, 0,
			(struct sockaddr *)&from, &from_length);
		if (length == SOCKET_ERROR)
			break;
		query[length] = 0;
		GS_ProcessQuery (query, &from);
	}
}

void SV_GameSpyShutdown (void)
{
	if (gs_query_socket != INVALID_SOCKET)
	{
		closesocket (gs_query_socket);
		gs_query_socket = INVALID_SOCKET;
	}
	if (gs_heartbeat_socket != INVALID_SOCKET)
	{
		closesocket (gs_heartbeat_socket);
		gs_heartbeat_socket = INVALID_SOCKET;
	}
#ifdef _WIN32
	if (gs_wsa_started)
	{
		WSACleanup ();
		gs_wsa_started = false;
	}
#endif
	gs_callbacks = false;
}

#endif /* KINGPIN_ENGINE */
