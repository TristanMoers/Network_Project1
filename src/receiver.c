#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "packet_interface.h"
#include "real_address.h"
#include "read_write_loop.h"
#include "create_socket.h"
#include "wait_for_client.h"

#define E_ARG 1
#define E_NOMEM 2
#define E_PORT 3
#define E_SOCKET 4
#define E_SENDACK 5
#define MAXPACKETSIZE 528
#define MAXWINDOW 31

int port = -1;
struct sockaddr_in6 addr;
int sfd;
int opt;
FILE *file = NULL;
int isFile = 0;
pkt_t *packet = NULL;
char raw_packet[MAXPACKETSIZE];

uint8_t seqnum;
int last_seqnum = -1;
uint32_t last_timestamp;
int type = -1;
int tr;

char filename[80];

int done = 0;
int end_seq = -1;
int nb = 0;

typedef struct {
    pkt_t   *pkt;
    int     ack;
} struct_window; 

struct_window **window;

int selective_loop(int i);
int send_ack_nack(int type, uint8_t seqnum, uint32_t timestamp);


// ================================================================
// =============================  Main  ===========================

int main(int argc, char **argv){

	// =================== Arguments Methods ======================

	if(argc < 3){
		fprintf(stderr, "Arguments missing !\n");
		return E_ARG;
	}

	if(argc >= 4){
		while((opt = getopt(argc, argv, "f:")) != -1){
			switch(opt){
				case 'f':
				isFile = 1;
				memcpy(filename, optarg, strlen(optarg));
				break;
				default:
				fprintf(stderr, "Unknown argument !\n");
				return E_ARG;
			}
		}
	}

	const char *ret = real_address(argv[argc - 2], &addr);
	if(ret != NULL){
		fprintf(stderr, "%s\n", ret);
	}

	port = atoi(argv[argc - 1]);
	if(port < 0 || port > 65535){
		fprintf(stderr, "Error port !\n");
		return E_PORT;
	}

	sfd = create_socket(&addr, port, NULL, -1);
	if (sfd > 0 && wait_for_client(sfd) < 0) {
			fprintf(stderr, "Could not connect the socket after the first message.\n");
			close(sfd);
			return E_SOCKET;
	}
	if(sfd < 0){
		return E_SOCKET;
	}

	if(isFile == 0){
		read_write_loop(sfd);
	}else{
		file = fopen(filename, "w");
		if(file == NULL){
			fprintf(stderr, "File error !\n");
			return E_NOMEM;
		}

		socklen_t address_length = sizeof(&addr);
		
		window = malloc(MAXWINDOW * sizeof(struct_window));


		// ================== Loop Receiving packet and Sending ACK ====================

		while(1){

			ssize_t nread = recvfrom(sfd, raw_packet, sizeof(raw_packet), 0, (struct sockaddr *) &addr, &address_length);

			if(nread > -1){
				packet = pkt_new();
				//if(packet == NULL){
				//	return E_NOMEM;
				//}
				pkt_status_code rv = pkt_decode(raw_packet, MAXPACKETSIZE, packet);

				if(rv == PKT_OK && nb <= MAXWINDOW){

					type = pkt_get_type(packet);
					tr = pkt_get_tr(packet);
					seqnum = pkt_get_seqnum(packet);
					last_timestamp = pkt_get_timestamp(packet);

					if(seqnum < (uint8_t)((last_seqnum + 1) % 256)){
						printf("Seq. %d déjà recu\n", seqnum);
						continue;
					}
					
					window[nb] = calloc(1, sizeof(struct_window));
					window[nb]->pkt = packet;

					window[nb]->ack = seqnum;
					nb++;
					
					// If it's last packet

					if(tr == 0){
						
						if(seqnum == (uint8_t)((last_seqnum + 1)%256)){
							last_seqnum++;
							if(type == PTYPE_DATA){
								fwrite(pkt_get_payload(packet), 1, pkt_get_length(packet), file);
								rv = send_ack_nack(PTYPE_ACK, (last_seqnum + 1)%256, last_timestamp);
								printf("Receive %d\n", (last_seqnum)%256);
								if(rv != PKT_OK){
									return rv;
								}
							}
							free(window[nb - 1]);
							break;
						}else{
							done = 1;
							end_seq = seqnum;
						}
						
					}else{

						if(type == PTYPE_DATA){
							if(seqnum == (uint8_t)((last_seqnum + 1)%256)){
								last_seqnum++;
								fwrite(pkt_get_payload(packet), 1, pkt_get_length(packet), file);
								free(window[nb-1]);
								nb--;
								nb = selective_loop(nb);
								rv = send_ack_nack(PTYPE_ACK, (last_seqnum + 1)%256, last_timestamp);
								printf("Receive %d\n", (last_seqnum)%256);
								printf("NB : %d\n", nb);
								if(rv != PKT_OK){
									return rv;
								}
								if(done == 1 && end_seq == last_seqnum%256){
									break;
								}
							}else{
								rv = send_ack_nack(PTYPE_NACK, seqnum, last_timestamp);
								if(rv != PKT_OK){
									return rv;
								}
							}
						}
					}

				}
			}
		}

		fclose(file);
	}

	free(window);
	close(sfd);

	printf("Transfert done!\n");

	return 0;
}

// ================================================================
// ================================================================



// ================== Loop to write in File =======================

int selective_loop(int i){
	int j;
	for(j = i;j>0;j--){
		if(window[j]->ack == (last_seqnum + 1)%256){
			last_seqnum++;
			fwrite(pkt_get_payload(window[j-1]->pkt), 1, pkt_get_length(window[j-1]->pkt), file);
			while(j <= i && j <= MAXWINDOW - 1){
				window[j-1] = window[j];
				j++;
			}
			i--;
			i = selective_loop(i);
			break;
		}
	}
	return i;
}


// ==================== Send ACK/NACK===============================

int send_ack_nack(int t, uint8_t seq, uint32_t timestamp){
	char raw_pkt[MAXPACKETSIZE];
	pkt_t *pkt = pkt_new();
	if(pkt == NULL){
		return E_NOMEM;
	}
	pkt_set_type(pkt, t);
	pkt_set_tr(pkt, 0);
	pkt_set_window(pkt, MAXWINDOW - nb);
	pkt_set_seqnum(pkt, seq);
	pkt_set_length(pkt, 0);
	pkt_set_timestamp(pkt, timestamp);
	pkt_set_payload(pkt, NULL, 0);

	size_t *len = malloc(sizeof(size_t));
    *len = sizeof(raw_pkt);

	pkt_status_code rv = pkt_encode(pkt, raw_pkt, len);
	if(rv == PKT_OK){
		int w = (int) write(sfd, raw_pkt,*len);
		if(w == -1){
			return E_SENDACK;
		}
	}
	return rv;
}
