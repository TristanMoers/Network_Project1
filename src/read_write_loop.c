#include "read_write_loop.h"

#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <poll.h>

#define MAX_SEGMENT_SIZE 1024

void read_write_loop(const int sfd){
	fd_set sfds;
	char buff[MAX_SEGMENT_SIZE];
	FD_ZERO(&sfds);

	while(1){
		FD_SET(STDIN_FILENO, &sfds);
		FD_SET(sfd, &sfds);

		select(sfd + 1, &sfds, NULL, NULL, NULL);

		if(FD_ISSET(STDIN_FILENO, &sfds)){
			ssize_t r = read(STDIN_FILENO, buff, MAX_SEGMENT_SIZE);

			if(r == EOF){
				break;
			}

			int w = (int) write(sfd, buff, r);

			if(w == -1){
				fprintf(stderr, "WRITE ERROR");
			}
		}else if(FD_ISSET(sfd, &sfds)){
			ssize_t r = read(sfd, buff, MAX_SEGMENT_SIZE);

			if(r == EOF){
				break;
			}

			int w = (int) write(STDOUT_FILENO, buff, r);
			if(w == -1){
				fprintf(stderr, "WRITE ERROR");
			}
		}
	}
}