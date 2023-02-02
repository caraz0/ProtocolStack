#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <rawnet.h>
#include <timerms.h>

#include "ipv4.h"
#include "udp.h"


int main(int argc, char *argv[]) {
    
    if ((argc <= 6) || (argc > 7)) {
        printf("       <string.txt>: IPV4_config.txt\n");
        printf("       <string.txt>: IPV4_route_table.txt\n");
        printf("        <ip>: IP dest\n");
        printf("        <puerto_in>: Puerto Origen\n");
        printf("        <puerto_out>: Puerto Destino\n");
        printf("       <int>: Bytes\n");
        exit(-1);
    }

    char *config_name = argv[1];
    char *route_table_name = argv[2];
    char *ip_str = argv[3];
    uint16_t port_in = atoi(argv[4]);
    uint16_t port_out = atoi(argv[5]);

    char *payload_len_str = argv[6];
    int payload_len_input = atoi(payload_len_str);

    if (payload_len_input > UDP_PACKET_LEN) {
        printf("Exceso de bytes\n");
        exit(-1);
    }

    ipv4_addr_t ip_addr;
    if (ipv4_str_addr(ip_str, ip_addr) != 0) {
        printf("Direccion ip erronea\n");
        exit(-1);
    }
    udp_layer_t *udp_layer = udp_open(port_in, config_name, route_table_name);
    
    unsigned char payload[payload_len_input];
    int i;
    for (i = 0; i < payload_len_input; i++) {
        payload[i] = (unsigned char) i;
    };

    printf("Paquete enviado\n");
    if (udp_send(udp_layer, ip_addr, port_out, payload, payload_len_input) == -1) {
        printf("Error envio\n");
        exit(-1);
    }
    printf("Escuchando a trama de vuelta\n");

    int timeout = -1;
    ipv4_addr_t sender;
    uint16_t *port = malloc(sizeof(uint16_t));

    int payload_len = udp_recv(udp_layer, timeout, sender, port, payload, payload_len_input);

    if (payload_len == -1) {
        printf("Error en la recepcion\n");
        exit(-1);
    } else if (payload_len > 0) {
        printf("Recibidos %d bytes\n", payload_len);
    }

    free(port);

    udp_close(udp_layer);

}
