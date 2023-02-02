#include "arp.h"
#include "ipv4.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <rawnet.h>
#include <timerms.h>

int main(int argc, char *argv[]) {
    char *myself = basename(argv[0]);
    if ((argc <= 2) || (argc > 3)) {
        printf("Uso: %s <iface> <ip> [<long>]\n", myself);
        printf("       <iface>: Interfaz por la que mandar request\n");
        printf("        <ip>: Ip destino\n");
        exit(-1);
    }
    char *iface_name = argv[1];
    ipv4_addr_t ipv4_addr_dest;

    if (ipv4_str_addr(argv[2], ipv4_addr_dest)) {
        printf("Direccion ip erronea\n");
        exit(-1);
    }

    eth_iface_t *iface = eth_open(iface_name);
    if (iface == NULL) {
        printf("Error al abrir la interfaz\n");
        exit(-1);
    }
    mac_addr_t mac;
    int resolve = arp_resolve(iface, IPv4_ZERO_ADDR, ipv4_addr_dest, mac);

    if (resolve == -2) {
        printf("Error al enviar el ARP Request\n");
        exit(-1);
    }
    char mac_str[MAC_ADDR_SIZE];
    mac_addr_str(mac, mac_str);
    printf("ip destino= %s -> Mac destino= %s\n", argv[2], mac_str);
    eth_close(iface);


}
