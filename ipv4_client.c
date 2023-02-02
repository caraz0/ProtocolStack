#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <rawnet.h>
#include <timerms.h>

#include "ipv4.h"


int main(int argc, char *argv[]) {

        if ((argc <= 5) || (argc > 6)) {
        printf("       <string.txt>: IPV4_config.txt\n");
        printf("       <string.txt>: IPV4_route_table.txt\n");
        printf("        <protocol>: Protocolo a utilizar(ej:0x06)\n");
        printf("        <ip>: IP dest\n");
        printf("        <int>: Bytes\n");
        exit(-1);
    }

    char *config_name = argv[1];
    char *route_table_name = argv[2];
    char *ipv4_type_str = argv[3];
    char *endptr;


    int ipv4_type_int = (int) strtol(ipv4_type_str, &endptr, 0);
    if ((*endptr != '\0') || (ipv4_type_int < 0) || (ipv4_type_int > 0x0000FFFF)) {
        printf("Protocolo no permitido\n");
        exit(-1);
    }
    uint16_t ipv4_protocol = (uint16_t) ipv4_type_int;


    char *ip_str = argv[4];

    ipv4_addr_t ip_addr;
    if (ipv4_str_addr(ip_str, ip_addr) != 0) {
        printf("Direccion ip erronea\n");
        exit(-1);
    }

    char *payload_len_str = argv[5];
    int payload_len_input = atoi(payload_len_str);

    if (payload_len_input > MRU){
        printf("Exceso de bytes\n");
        exit(-1);
    }

    ipv4_layer_t *ip_layer = ipv4_open(config_name, route_table_name);

    if (ip_layer == NULL) {
        printf("Error al leer el txt\n");
        exit(-1);
    }
    unsigned char payload[payload_len_input];
    int i;
    for (i = 0; i < payload_len_input; i++) {
        payload[i] = (unsigned char) i;
    };
    if (ipv4_send(ip_layer, ip_addr, ipv4_protocol, payload, payload_len_input) == -1) {
        printf("Error en el envio\n");
        exit(-1);
    }
    printf("Paquete enviado\n");

    unsigned char buffer[MRU];
    ipv4_addr_t src;

    printf("Esperando a la recepcion\n");
    int payload_len = ipv4_recv(ip_layer, 0X45, buffer, src, MRU, -1);

    if (payload_len == -1) {
        printf("Error en la recepcion\n");
        exit(-1);
    } else if (payload_len > 0) {
        printf("Paquete recibido\n");

    }

    ipv4_close(ip_layer);

}
