#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <rawnet.h>
#include <timerms.h>
#include <arpa/inet.h>

#include "ipv4.h"


int main(int argc, char *argv[]) {

    

    if ((argc <= 3) || (argc > 4)) {
        printf("       <string.txt>: IPV4_config.txt\n");
        printf("       <string.txt>: IPV4_route_table.txt\n");
        printf("        <protocol>: Protocolo a utilizar(0x06)\n");
        exit(-1);
    }

    

    char *config_name = argv[1];
    char *route_table_name = argv[2];

    

    char *ipv4_type_str = argv[3];
    char *endptr;

    int ipv4_type_int = (int) strtol(ipv4_type_str, &endptr, 0);
    if ((*endptr != '\0') || (ipv4_type_int < 0) || (ipv4_type_int > 0x0000FFFF)) {
        printf("Error Protocolo: Ej:0x06\n");
        exit(-1);
    }
    uint16_t ipv4_protocol = (uint16_t) ipv4_type_int;
    
    ipv4_layer_t *ip_layer = ipv4_open(config_name, route_table_name);

    if (ip_layer == NULL) {
        printf("Error al leer el txt\n");
        exit(-1);
    }

    unsigned char buffer[IPV4_FRAME_LEN];
    ipv4_addr_t src_addr;
    int payload_len;

    while (1) {

        long int timeout = -1;

        printf("Escuchando tramas ipv4\n");
        payload_len = ipv4_recv(ip_layer, ipv4_protocol, buffer, src_addr, MRU, timeout);

        if (payload_len == -1) {
            printf("Error en la recepcion\n");
            exit(-1);
        }

        printf("Paquete recibido\n");
        break;

    }

    printf("Enviamos bytes de vuelta al cliente\n");

    ipv4_send(ip_layer, src_addr, ipv4_protocol, buffer, payload_len);

    ipv4_close(ip_layer);
}
