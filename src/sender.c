#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <zlib.h>
#include <netdb.h>
#include <sys/time.h>


#include "real_address.h"
#include "create_socket.h"
#include "read_write_loop.h"
#include "wait_for_client.h"
#include "packet_interface.h"


typedef struct {
    pkt_t   *pkt;
    int     ack;
} struct_window;

int check_port(const char *string);
int file_size(FILE *f);
int nb_pkt(const ssize_t length);
void extract_file_data(char **payload, FILE *f, int file_pos, size_t *payload_length);
int fill_window(struct_window **window,int seqnum,int window_size,int nb_packets,FILE *f,int *file_pos, size_t *remaining_length);
int fill_window_last_packet(struct_window **window,int seqnum);
int send_packet_from_window(int sfd, struct_window **window, int send_packet);
int send_one_packet_from_window(int sfd, pkt_t *pkt);
void selective_loop(int nb_packets, FILE *f, int sfd);


// ================================================================
// =============================  Main  ===========================

int main(int argc, char **argv) {

    int c, is_file = 0;
    char filename[BUFSIZ];
    FILE *f;
    size_t length = 0;

    
    // =================== Arguments Methods ======================

    if(argc >= 3) {
        while ((c = getopt(argc, argv, "f:")) != -1) {
            switch (c) {
            case 'f':
                is_file = 1;
                memcpy(filename, optarg, strlen(optarg));
                break;
            default:
                fprintf(stderr, "argument unknown\n");
                return EXIT_FAILURE;
            }
        }
     }

    argc -= optind;
    argv += optind;


    if (argc < 2) {
        fprintf(stderr, "missing argument\n");
        return EXIT_FAILURE;
    }

    struct stat file;

    if(stat(filename, &file) == -1 && is_file == 1) {
        fprintf(stderr, "file not found\n");
        return EXIT_FAILURE;
    }

    int port = check_port(argv[1]);
    if(check_port(argv[1]) == -1) {
        fprintf(stderr, "invalid port\n");
        return EXIT_FAILURE;
    }


    struct sockaddr_in6 addr;
    const char *err = real_address(argv[0], &addr);
    if (err) {
        fprintf(stderr, "Could not resolve hostname or ip : %s\n", err);
        return EXIT_FAILURE;
    }


    int sfd = create_socket(NULL, -1, &addr, port);
    if (sfd < 0 && wait_for_client(sfd) < 0) {
        return EXIT_FAILURE;
    }



    // ==================== File operations =======================
    
    if(is_file == 1){
        f = fopen(filename, "r");
    }else{
        read_write_loop(sfd);
        f = stdin;
    }

    length = file_size(f);
    int nb_packets = nb_pkt(length);


    // ====================== Sender Loop =========================

    selective_loop(nb_packets, f, sfd);

    fclose(f);
    close(sfd);

    return 0;
}

// ================================================================
// ================================================================





//Check port and convert it to int
int check_port(const char *string) {
    int val = -1;
    if ((val = strtol(string, NULL, 10)) == 0) {
        return -1;
    }
    return (int)val;
}


//Take the size of file
int file_size(FILE *f) {
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    return size;
}


//Calculate total number of packet to send 
int nb_pkt(const ssize_t length)
{
    unsigned int nb_packets = 0;
    unsigned int payload_space = (length / MAX_PAYLOAD_SIZE);

    if (payload_space == 0) {
        nb_packets = 1;
    } else {
        nb_packets = payload_space;

        if ((length % MAX_PAYLOAD_SIZE) > 0) {
            nb_packets++;
        }
    }
    return nb_packets;
}


void extract_file_data(char **payload, FILE *f, int file_pos, size_t *payload_length) {

    *payload = realloc(*payload, *payload_length);
    memset(*payload, '\0', *payload_length);
    fseek(f, file_pos, SEEK_SET);
    size_t read = fread(*payload, sizeof(char) , *payload_length, f);
    if (read < *payload_length) {
        *payload_length = read;
    }
}


// =======================================================================
// =======================  Fill All Packets In Window  ==================

int fill_window(struct_window **window,int seqnum,int window_size,int nb_packets,FILE *f,int *file_pos, size_t *remaining_length) {
    size_t payload_length = MAX_PAYLOAD_SIZE;
    char *payload = NULL;
    int i, seq;
    for(i=0; i < nb_packets; i++) {
        window[i] = calloc(1, sizeof(struct_window));

        window[i]->pkt = pkt_new();
        pkt_set_type(window[i]->pkt, PTYPE_DATA);
        pkt_set_window(window[i]->pkt, window_size);
        pkt_set_seqnum(window[i]->pkt, seqnum+i);
        pkt_set_timestamp(window[i]->pkt, 0);
        
        pkt_set_tr(window[i]->pkt, 0);

        if(payload_length < *remaining_length)
            payload_length = MAX_PAYLOAD_SIZE;
        else
            payload_length = *remaining_length;
        extract_file_data(&payload, f, *file_pos, &payload_length);
        *file_pos += payload_length;
        *remaining_length -= payload_length;

        pkt_set_payload(window[i]->pkt, payload, payload_length);
        pkt_set_crc2(window[i]->pkt, pkt_gen_crc2(window[i]->pkt));

        pkt_set_crc1(window[i]->pkt, pkt_gen_crc1(window[i]->pkt));
        window[i]->ack = 0;

        seq = seqnum + i;
        
    }
    return seq;

}

// ========================================================================
// =======================  Fill One Packet in Window  ====================

int fill_window_last_packet(struct_window **window,int seqnum) {
    int seq = seqnum + 1;

    window[0] = calloc(1, sizeof(struct_window));
    window[0]->pkt = pkt_new();

    pkt_set_type(window[0]->pkt, PTYPE_DATA);
    pkt_set_tr(window[0]->pkt, 0);
    pkt_set_window(window[0]->pkt, 1);
    pkt_set_seqnum(window[0]->pkt, seq);
    pkt_set_timestamp(window[0]->pkt, 0);
    pkt_set_payload(window[0]->pkt, NULL, 0);
    pkt_set_length(window[0]->pkt, 0);
    window[0]->ack = 0;

    return seq;
}


// =======================================================================
// ======================  Sending All Packets in Window =================

int send_packet_from_window(int sfd, struct_window **window, int send_packet) {
    size_t buf_length = 528;
    char *buf = malloc(buf_length);
    int r = 0;
    int i;
    for(i = 0; i < send_packet; i++) {
        if(window[i]->ack == 0) {
            memset(buf, '\0', buf_length);
            if (pkt_encode(window[i]->pkt, buf, &buf_length) != PKT_OK) {
                printf("[ERROR]fail to encode pkt %d\n", i);
                r = -1;
            }

            if (send(sfd, buf, buf_length, 0) == -1) {
                //printf("[ERROR]Failed to send package %lu\n", i);
                printf("%s\n", strerror(errno));
                r = -1;
            } else
               printf("[OK]Send %d packet\n", i);
        }
    }
    free(buf);
    buf = NULL;
    return r;
}

  

// =======================================================================
// =========================  Sending One Packet  ========================

int send_one_packet_from_window(int sfd, pkt_t *pkt) {
    size_t buf_length = 528;
    char *buf = malloc(buf_length);
    int r = 0;
    memset(buf, '\0', buf_length);
    if (pkt_encode(pkt, buf, &buf_length) != PKT_OK) {
        printf("[ERROR]fail to encode pkt\n");
        r = -1;
    }
    if (send(sfd, buf, buf_length, 0) == -1) {
        printf("[ERROR]Failed to send package\n");
        r = -1;
    } else
        printf("[OK]Send packet seq=%d\n", pkt_get_seqnum(pkt));
    free(buf);
    buf = NULL;
    return r;
}


// =======================================================================
// =============================  Loop Sender  ===========================

void selective_loop(int nb_packets, FILE *f, int sfd) {
    int send = 1;
    int recieve_ack = 0;
    int window_size = 31;
    int send_packet;
    int ack_packet;
    int current_packet = 0;
    int previous_seqnum = 0;
    int seqnum_ack;
    int last = 0;
    size_t remaining_length = file_size(f);

    int seqnum = 0;
    int file_pos = 0;

    fd_set readfs;
    struct timeval  t;
    t.tv_sec = 5;
    t.tv_usec = 0;

    // ====================== Loop Sending Packets in Window ===================

    while(send) {

        if(last) {
            nb_packets = 1;
            send = 1;
        }

        if(nb_packets <= window_size) {
            current_packet = nb_packets;
            send_packet = nb_packets;
            ack_packet = nb_packets;
        }
        else {
            current_packet = window_size;
            send_packet = window_size;
            ack_packet = window_size;
        }

        struct_window **window = malloc(send_packet * sizeof(struct_window));

        if(last)
            seqnum = fill_window_last_packet(window, seqnum);
        else
            seqnum = fill_window(window, seqnum, window_size, send_packet, f, &file_pos, &remaining_length);

        nb_packets -= send_packet;
        if(nb_packets == 0){
            last = 1;
        }
        
        printf("[INFO]Sending %d packets\n", send_packet);
        send_packet_from_window(sfd,window,send_packet);
       

       // =========================== Loop Waiting Ack  ========================

        while(recieve_ack < send_packet) {

            int ret = 0;
            FD_ZERO(&readfs);
            FD_SET(sfd, &readfs);
   
            if((ret = select(sfd + 1, &readfs, NULL, NULL, &t)) < 0) {
                printf("[ERROR]Erreur Select\n");
            } 
       
            if(FD_ISSET(sfd, &readfs)) {
                pkt_t *ack = pkt_new();
                char ack_buf[1024];
                int r = read(sfd, ack_buf, sizeof(ack_buf));
                if(r == -1) {
                    printf("[ERROR] Can't read acks\n");
                }

                if (pkt_decode(ack_buf, 1024, ack) != PKT_OK) 
                        continue;

                if(pkt_get_type(ack) == PTYPE_ACK) {

	                seqnum_ack = pkt_get_seqnum(ack);
	                printf("[OK] Ack seqnum : %d\n", seqnum_ack);
	                int i;
	                for(i = 0; i < send_packet; i++) {
	                    if(pkt_get_seqnum(window[i]->pkt) < seqnum_ack)
	                        window[i]->ack = 1;
	                }

	                int diff_seq = seqnum_ack - previous_seqnum;
	                int j, d;
	                if(diff_seq != 1) {
	                    printf("[INFO] Acks not retrieved\n");
	                   for(j = previous_seqnum + 1; j <= seqnum_ack; j++) {
	                        for(i = 0; i < send_packet; i++) {
	                            if(pkt_get_seqnum(window[i]->pkt) == j && window[i]->ack == 0)
	                                d = send_one_packet_from_window(sfd, window[i]->pkt);
	                        } 
	                    }
	                }
	            }

	            if(pkt_get_type(ack) == PTYPE_NACK) {

                    int j, d, i;
                    seqnum_ack = pkt_get_seqnum(ack);
                    printf("[INFO] Nack seqnum : %d\n", seqnum_ack);
                    printf("[INFO] Previous seq : %d\n", previous_seqnum);
                    for(j = previous_seqnum; j <= seqnum_ack -1; j++) {
                        for(i = 0; i < send_packet; i++) {
                            if(pkt_get_seqnum(window[i]->pkt) == j && window[i]->ack == 0) {
                                d = send_one_packet_from_window(sfd, window[i]->pkt);
                                window[i]->ack = 1;
                            }
                        } 
                    }
                }
                
            }
            previous_seqnum = seqnum_ack;
			int i;
            for(i = recieve_ack; i < send_packet; i++) {
                if(window[i]->ack == 1)
                   recieve_ack++;
            }

        }    

        recieve_ack = 0;
        window_size = 32;
        seqnum = seqnum + 1;
        free(window);
        window = NULL;

    }


}
