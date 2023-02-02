#include "arp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <rawnet.h>
#include <arpa/inet.h>

#define IP_PROTOCOL 0x0800 
#define HARDW_TYPE 0x0001 
#define ARP_TYPE 0x0806 
#define ARP_REQUEST 0x0001 
#define ARP_REPLY 0x0002 

mac_addr_t UNKNOW_MAC = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 


typedef struct arp_message {
    uint16_t hard_addr; 
    uint16_t protocol_type;
    uint8_t hard_size;
    uint8_t protocol_length;
    uint16_t opcode;
    mac_addr_t mac_sender;
    ipv4_addr_t ip_sender;
    mac_addr_t mac_target;
    ipv4_addr_t ip_target;

} arp_message_t;


int arp_resolve(eth_iface_t *iface, ipv4_addr_t src, ipv4_addr_t destino, mac_addr_t mac) {

    //Rellenamos nuestro arp_message a enviar via eth
    arp_message_t arp_payload;
    arp_payload.hard_addr = htons(HARDW_TYPE);
    arp_payload.protocol_type = htons(IP_PROTOCOL); 
    arp_payload.hard_size = 6;
    arp_payload.protocol_length = 4;
    arp_payload.opcode = htons(ARP_REQUEST); 
    eth_getaddr(iface, arp_payload.mac_sender); 
    memcpy(arp_payload.ip_sender, src,
           IPv4_ADDR_SIZE); 
    memcpy(arp_payload.mac_target, UNKNOW_MAC, MAC_ADDR_SIZE); 
    memcpy(arp_payload.ip_target, destino, IPv4_ADDR_SIZE); 
    

    eth_send(iface, MAC_BCAST_ADDR, ARP_TYPE, (unsigned char *) &arp_payload,sizeof(arp_payload));
        
    printf("Arp request enviado\n");

    unsigned char buffer[sizeof(arp_message_t)];
    arp_message_t *arp_message = NULL;

    while (1) {

        
        //comprobamos que es tipo arp
        int buffer_len = eth_recv(iface, mac, ARP_TYPE, buffer, sizeof(arp_message_t), -1);

        if (buffer_len < sizeof(arp_message_t)) {
            continue;
        }
        arp_message = (arp_message_t *) buffer;

        //comprobacion que es la ip que queremos
        if (ntohs(arp_message->opcode) == ARP_REPLY && memcmp(arp_message->ip_sender, destino, IPv4_ADDR_SIZE) == 0) {

            memcpy(mac, arp_message->mac_sender, MAC_ADDR_SIZE);
            printf("ARP reply recibido\n");
            return 1;
        }

    }

}
