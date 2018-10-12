#include "packet_interface.h"
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

/* Extra #includes */
/* Your code will be inserted here */


struct __attribute__((__packed__)) pkt {
    struct {
        uint8_t window : 5;
       uint8_t tr : 1;
       uint8_t type : 2;
        uint8_t seqnum;
        uint16_t length;
       uint32_t timestamp;
       uint32_t crc1;
    } header;
    char *payload;
    uint32_t crc2;
};

#define MAX_PKT_SIZE        sizeof(pkt_t) + MAX_PAYLOAD_SIZE - sizeof(char *);

pkt_t * pkt_new() {
    return calloc(1, sizeof(pkt_t));
}

void pkt_del(pkt_t *pkt) {
    if (pkt->payload != NULL) {
        free(pkt->payload);
    }
    free(pkt);
}


ptypes_t pkt_get_type(const pkt_t *pkt) {
    return pkt->header.type;
}

uint8_t pkt_get_window(const pkt_t *pkt) {
    return pkt->header.window;
}

uint8_t pkt_get_seqnum(const pkt_t *pkt) {
    return pkt->header.seqnum;
}

uint16_t pkt_get_length(const pkt_t *pkt) {
    return ntohs(pkt->header.length);
}

uint32_t pkt_get_timestamp(const pkt_t *pkt) {
    return pkt->header.timestamp;
}

uint32_t pkt_get_crc1(const pkt_t *pkt) {
    return pkt->header.crc1;
}

uint32_t pkt_get_crc2(const pkt_t *pkt) {
    return pkt->crc2;
}

uint8_t pkt_get_tr(const pkt_t *pkt) {
    return pkt->header.tr;
}

const char * pkt_get_payload(const pkt_t *pkt) {
    if (pkt->payload == NULL) {
        return NULL;
    }
    return pkt->payload;
}



pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type) {
    if (type != PTYPE_DATA && type != PTYPE_ACK && type != PTYPE_NACK) {
        return E_TYPE;
    } else {
        pkt->header.type = type;
        return PKT_OK;
    }
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window) {
    if (window > MAX_WINDOW_SIZE) {
        return E_WINDOW;
    } else {
        pkt->header.window = window;
        return PKT_OK;
    }
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum) {
    pkt->header.seqnum = seqnum;
    return PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length) {
    if (length > MAX_PAYLOAD_SIZE) {
        return E_LENGTH;
    } else {
        pkt->header.length = htons(length);
        return PKT_OK;
    }
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp) {
    pkt->header.timestamp = timestamp;
    return PKT_OK;
}

pkt_status_code pkt_set_crc1(pkt_t *pkt, const uint32_t crc) {
    pkt->header.crc1 = crc;
    return PKT_OK;
}

pkt_status_code pkt_set_crc2(pkt_t *pkt, const uint32_t crc) {
    pkt->crc2 = crc;
    return PKT_OK;
}

pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length) {
    /* null payload has a length of 0, usually for session termination */
    if (data == NULL) {
        pkt_set_length(pkt, 0);
        pkt->payload = NULL;
        return PKT_OK;
    }

    pkt->payload = malloc(length);
    if (pkt->payload == NULL) {
        return E_NOMEM;
    }

    memcpy(pkt->payload, data, length);

    pkt_status_code ret = pkt_set_length(pkt, length);
    if (ret != PKT_OK) {
        return ret;
    }

    return PKT_OK;
}

pkt_status_code pkt_set_tr(pkt_t *pkt, const uint8_t tr) {
    pkt->header.tr = tr;
    return PKT_OK;
}


uint32_t pkt_gen_crc1(const pkt_t *pkt) {
    uLong crc = crc32(0L, Z_NULL, 0);
    uint8_t tr = pkt_get_tr(pkt);
    pkt_set_tr((pkt_t*) pkt, 0);
    crc = crc32(crc, (const Bytef *)pkt, sizeof(pkt->header) - sizeof(uint32_t));
    pkt_set_tr((pkt_t*) pkt, tr);
    return htonl((uint32_t)crc);
}

uint32_t pkt_gen_crc2(const pkt_t *pkt) {
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)pkt->payload, pkt_get_length(pkt));
        return htonl((uint32_t)crc);
}



pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt) {
    int read = 0;

    /* copy the header */
    memcpy(pkt, data, sizeof(pkt->header));
    read += sizeof(pkt->header);

    if (len < (sizeof(pkt_t) + pkt_get_length(pkt) - sizeof(pkt->payload))) {
        return E_NOMEM;
    }


    /* copy the payload */
    if ((pkt_get_length(pkt) > 0)) {
        pkt_set_payload(pkt, data + read, pkt_get_length(pkt));
        read += pkt_get_length(pkt);
        
            /* copy the crc 2*/
            //uint32_t crc = 0;
            memcpy(&(pkt->crc2), data + read, sizeof(uint32_t));
            read += sizeof(pkt->crc2);
	       

            /* verify the crc */
            if (pkt_gen_crc2(pkt) != pkt_get_crc2(pkt)) {
            return E_UNCONSISTENT;
            }
    }


        /* verify the crc */
    if (pkt_gen_crc1(pkt) != pkt_get_crc1(pkt)) {
        return E_UNCONSISTENT;
    }


    return PKT_OK;
}


pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len) {
    /* payload length */
    uint16_t plen = pkt_get_length(pkt);
    //printf("%lu et %lu\n", *len, sizeof(pkt_t) + plen - sizeof(char *));
    if (*len < sizeof(pkt_t) + plen - sizeof(char *)) {
        return E_NOMEM;
    }

    /* encode the header */
    *len = sizeof(pkt->header) - sizeof(uint32_t);
    memcpy(buf, pkt, *len);


    /* copy the crc 1*/
    uint32_t crc1 = pkt_gen_crc1(pkt);
    pkt_set_crc1((pkt_t*) pkt, crc1);
    memcpy(buf + *len, &pkt->header.crc1, sizeof(pkt->header.crc1));
    *len += sizeof(pkt->header.crc1);

    /* copy the payload */
    if ((pkt_get_tr(pkt)) || (plen > 0)) {

        memcpy(buf + *len, pkt->payload, plen);
        
        *len += plen;
        
        /* copy the crc 2*/
        uint32_t crc2 = pkt_gen_crc2(pkt);
        pkt_set_crc2((pkt_t*) pkt, crc2);
        memcpy(buf + *len, &pkt->crc2, sizeof(pkt->crc2));
        *len += sizeof(pkt->crc2);
    }


    return PKT_OK;
}

