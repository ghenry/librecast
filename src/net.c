#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "config.h"
#include "errors.h"
#include "log.h"
#include "net.h"

int sock;
struct addrinfo *castaddr;

int net_multicast_init()
{
	int e = 0, errsv;
        int loop = 1; /* 0 = off, 1 = on (default) */
        int ttl = 1;
	char *addr = config_get("castaddr");
	char *port = config_get("castport");

	/* resolve destination address */
	if (net_multicast_getaddrinfo(addr, port, &castaddr) != 0) {
                goto net_multicast_init_fail;
        }

        /* create socket */
        logmsg(LOG_DEBUG, "creating datagram socket");
        sock = socket(castaddr->ai_family, castaddr->ai_socktype, 0);
        if (sock == -1) {
                goto net_multicast_init_fail;
        }

        /* set TTL */
        logmsg(LOG_DEBUG, "setting multicast TTL");
        if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *)&ttl,
                                sizeof(ttl)) != 0)
        {
                goto net_multicast_init_fail;
        }

        logmsg(LOG_DEBUG, "setting loopback");
        if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *)&loop,
                                sizeof(loop)) != 0)
        {
                goto net_multicast_init_fail;
        }

	return 0;

net_multicast_init_fail:
	errsv = errno;
	print_error(e, errsv, "net_multicast_init");
	config_free();
	_exit(e);
}

int net_multicast_getaddrinfo(const char *node, const char *service,
		struct addrinfo **res)
{
	struct addrinfo hints = { 0 };
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_NUMERICHOST;
        logmsg(LOG_DEBUG, "resolving multicast address");
        return getaddrinfo(node, service, &hints, res);
}

int net_multicast_listen()
{
	int e = 0, errsv;
	struct addrinfo *res;
	char *addr = config_get("castaddr");
	char *port = config_get("castport");
	struct addrinfo hints = { 0 };
	struct addrinfo *localaddr;
	struct ipv6_mreq req;

	if (net_multicast_getaddrinfo(addr, port, &res) != 0) {
                goto net_multicast_listen_fail;
        }

	hints.ai_family = res->ai_family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	/* find local address to bind to */
	if (getaddrinfo(NULL, port, &hints, &localaddr) != 0) {
                goto net_multicast_listen_fail;
	}

	/* create datagram socket */
	sock = socket(localaddr->ai_family, localaddr->ai_socktype, 0);
	if (sock == -1) {
                goto net_multicast_listen_fail;
	}

	/* bind to multicast port */
	if (bind(sock, localaddr->ai_addr, localaddr->ai_addrlen) != 0) {
                goto net_multicast_listen_fail;
	}

	memcpy(&req.ipv6mr_multiaddr,
			&((struct sockaddr_in6*)(res->ai_addr))->sin6_addr,
			sizeof(req.ipv6mr_multiaddr));

	/* ifindex = if_nametoindex("eth0"); */
	req.ipv6mr_interface = 0; /* default interface */

	/* join multicast */
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *)&req,
				sizeof(req)) != 0)
	{
                goto net_multicast_listen_fail;
	}

	freeaddrinfo(localaddr);
	freeaddrinfo(res);

	for (;;) {
		time_t timer;
		char recv[1024];
		int l;

		if ((l = recvfrom(sock, recv, sizeof(recv)-1, 0, NULL, 0)) < 0){
			goto net_multicast_listen_fail;
		}
		recv[l] = '\0';
		time(&timer);
		printf("%.*s : %s\n", (int)strlen(ctime(&timer)) - 1,
				ctime(&timer), recv);
	}

	return 0;

net_multicast_listen_fail:
	errsv = errno;
	print_error(e, errsv, "net_multicast_listen");
	config_free();
	_exit(e);
}

int net_multicast_send(char *msg)
{
	int e = 0, errsv;

	logmsg(LOG_DEBUG, "Sending datagram");
	if (sendto(sock, msg, sizeof(char) * strlen(msg) + 1, 0,
				castaddr->ai_addr, castaddr->ai_addrlen) < 0)
	{
		goto net_multicast_send_fail;
	}

	return 0;

net_multicast_send_fail:
	errsv = errno;
	print_error(e, errsv, "net_multicast_init");
	net_free();
	return ERROR_NET_SEND;
}

int net_free()
{
	freeaddrinfo(castaddr);
	return 0;
}