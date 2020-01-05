
#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <unistd.h>
#include <pthread.h>
#else
#define  _WINSOCK_DEPRECATED_NO_WARNINGS
#define  _CRT_SECURE_NO_WARNINGS
#if defined(WIN32) && !defined(_WIN32_WCE) && !defined(__CYGWIN__)
#if !(defined(_WINSOCKAPI_) || defined(_WINSOCK_H) || defined(__LWIP_OPT_H__))
/* The check above prevents the winsock2 inclusion if winsock.h already was
   included, since they can't co-exist without problems */
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#endif
#include <windows.h>
#include <stdint.h>
#endif // !WIN32

#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// program yaamp.com:port coinid blockhash

int main(int argc, char **argv)
{
	if(argc < 4)
	{
		printf("usage: blocknotify server:port coinid blockhash\n");
		return 1;
	}

	char *p = strchr(argv[1], ':');
	if(!p)
	{
		printf("usage: blocknotify server:port coinid blockhash\n");
		return 1;
	}

	int port = atoi(p+1);
	*p = 0;

	int coinid = atoi(argv[2]);
	char blockhash[1024];
	strncpy(blockhash, argv[3], 1024);

#ifndef WIN32
	int sock = socket(AF_INET, SOCK_STREAM, 0);
#else
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		wprintf(L"WSAStartup function failed with error: %d\n", iResult);
		return 1;
	}
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif // !WIN32
	
	if(sock <= 0)
	{
		printf("error socket %s id %s\n", argv[1], argv[2]);
		return 1;
	}

	struct hostent *ent = gethostbyname(argv[1]);
	if(!ent)
	{
		printf("error gethostbyname %s id %s\n", argv[1], argv[2]);
		return 1;
	}

	struct sockaddr_in serv;

	serv.sin_family = AF_INET;
	serv.sin_port = htons(port);

#ifndef WIN32
	bcopy((char*)ent->h_addr, (char*)&serv.sin_addr.s_addr, ent->h_length);
#else
	memcpy((char*)&serv.sin_addr.s_addr,(char*)ent->h_addr, ent->h_length);
#endif // !WIN32

	int res = connect(sock, (struct sockaddr*)&serv, sizeof(serv));
	if(res < 0)
	{
		printf("error connect %s id %s\n", argv[1], argv[2]);
#ifdef WIN32
		WSACleanup();
#endif
		return 1;
	}

	char buffer[1024];
	sprintf(buffer, "{\"id\":1,\"method\":\"mining.update_block\",\"params\":[\"tu8tu5\",%d,\"%s\"]}\n", coinid, blockhash);

	send(sock, buffer, strlen(buffer), 0);
#ifndef WIN32
	close(sock);
#else
	closesocket(sock);
#endif
	//fprintf(stdout, "notify %s\n", buffer);
#ifdef WIN32
	WSACleanup();
#endif	
	return 0;
}



