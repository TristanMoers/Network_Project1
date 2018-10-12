#include "real_address.h"

#include <netdb.h>
#include <string.h>

const char * real_address(const char *address, struct sockaddr_in6 *rval){
	struct addrinfo hints;
	struct addrinfo *res;
	int s;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET6; 		// IPv6
	hints.ai_protocol = IPPROTO_UDP; 	// UDP
	hints.ai_socktype = SOCK_DGRAM; 	// Datagram socket (UDP => datagram protocol)
	hints.ai_flags = AI_PASSIVE;		// For wildcard ip address
	hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(address, NULL, &hints, &res);

    if(s != 0){
    	return gai_strerror(s);
    }

    struct sockaddr_in6 * addr = (struct sockaddr_in6 *)res->ai_addr;

    memcpy(rval, addr, res->ai_addrlen);

    freeaddrinfo(res);
    return NULL;
}