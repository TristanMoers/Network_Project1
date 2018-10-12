#include "create_socket.h"

#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>

int create_socket(struct sockaddr_in6 *source_addr, int src_port, struct sockaddr_in6 *dest_addr, int dst_port){
	int sfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	if(sfd == -1){
		fprintf(stderr, "Can't create a socket\n");
		return -1;
	}

	if(source_addr != NULL){

		if(src_port > 0){
			source_addr->sin6_port = htons(src_port);
		}else{
			fprintf(stderr, "Source port must be positive\n");
			return -1;
		}

		if(bind(sfd, (struct sockaddr *)source_addr, sizeof(struct sockaddr_in6)) == -1){
			fprintf(stderr, "Can't bind\n");
			return -1;
		}

	}

	if(dest_addr != NULL){

		if(dst_port > 0){
			dest_addr->sin6_port = htons(dst_port);
		}else{
			fprintf(stderr, "Destination port must be positive\n");
			return -1;
		}

		if(connect(sfd, (struct sockaddr *)dest_addr, sizeof(struct sockaddr_in6)) == -1){
			fprintf(stderr, "Can't connect\n");
			return -1;
		}

	}

	return sfd;
}