#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <rawnet.h>
#include <timerms.h>
#include <arpa/inet.h>

#include "ipv4.h"
#include "udp.h"


int main(int argc, char *argv[]) {
   

    if ((argc <= 3) || (argc > 4)) {
        printf("       <string.txt>: IPV4_config.txt\n");
        printf("       <string.txt>: IPV4_route_table.txt\n");
        printf("       <int>: Puerto por el que escuchamos\n");
        exit(-1);
    }

    char *config_name = argv[1];
    char *route_table_name = argv[2];
    uint16_t port_in = atoi(argv[3]);

    udp_layer_t *udp_layer = udp_open(port_in ,config_name, route_table_name);

    unsigned char buffer[UDP_PACKET_LEN];
    uint16_t port;
    ipv4_addr_t sender;
	int payload_len;
   

    while (1) {

        

        long int timeout = -1;

        printf("Esperando Recepcion\n");

        payload_len = udp_recv(udp_layer, timeout, sender, &port, buffer, UDP_PACKET_LEN);

        printf("Recibidos %d bytes\n", payload_len);
        break;

    }

    printf("Enviando paquetes de vuelta\n");

	printf("%i\n", port);

    udp_send(udp_layer, sender, port, buffer, payload_len);

    udp_close(udp_layer);
}
