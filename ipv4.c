#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <timerms.h>

#include "ipv4.h"
#include "ipv4_route_table.h"
#include "ipv4_config.h"
#include "arp.h"

//Estructura que guarda toda la informacion de la interfaz
typedef struct ipv4_layer {

    eth_iface_t *iface;
    ipv4_addr_t addr;
    ipv4_addr_t network;
    ipv4_route_table_t *routing_table;

} ipv4_layer_t;

/* Dirección IPv4 a cero: "0.0.0.0" */
ipv4_addr_t IPv4_ZERO_ADDR = {0, 0, 0, 0};
ipv4_addr_t IPv4_MULTICAST_ADDR = {224, 0, 0, 0};
ipv4_addr_t IPv4_MULTICAST_NETWORK = {240, 0, 0, 0};
//Estructura para la trama IPV4 (CONSULTAR)
typedef struct ipv4_message {

    uint8_t version;
    uint8_t type;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_offset;
    uint8_t TTL;
    uint8_t protocol;
    uint16_t checksum;
    ipv4_addr_t source;
    ipv4_addr_t dest;
    unsigned char data[MRU];

} ipv4_message_t;

ipv4_layer_t *ipv4_open(char *file_config, char *file_conf_route) {

    //Reservamos memoria para ifname y el puntero al struct ipv4_layer

    char ifname[IFACE_NAME_MAX_LENGTH];
    ipv4_layer_t *ipv4_layer = malloc(sizeof(ipv4_layer_t));

    //Leemos el fichero config y guardamos el nombre de la interfaz, la ip y la mascara asociadas a estas

    ipv4_config_read(file_config, ifname, ipv4_layer->addr, ipv4_layer->network);

    //Reservamos memoria para una routing table y la guardamos en la variable
    ipv4_layer->routing_table = ipv4_route_table_create();
    ipv4_route_table_read(file_conf_route, ipv4_layer->routing_table);


    //Hacemos un open de la interfaz con nombre ifname
    ipv4_layer->iface = eth_open(ifname);

    return ipv4_layer;
}


int ipv4_send(ipv4_layer_t *layer, ipv4_addr_t dst, uint8_t protocol,
              unsigned char *payload, int payload_len) {
   
        ipv4_route_t *next_jump = ipv4_route_table_lookup(layer->routing_table, dst);

        if (next_jump->gateway_addr == NULL) {
            fprintf(stderr, "No hay ruta disponible para transmitir los datos.\n");
            return -1;
        }
    
    else if (memcmp(next_jump->gateway_addr, IPv4_ZERO_ADDR, sizeof(ipv4_addr_t)) == 0) {
        printf("El siguiente salto es el propio destino\n");
        memcpy(next_jump->gateway_addr, dst, sizeof(ipv4_addr_t));
    }
    mac_addr_t your_mac = "\0";

    /*IP HEADER*/

    ipv4_message_t ipv4_frame;
    int ipv4_frame_len = payload_len + IPV4_HEADER_SIZE;

    //IP HEADER VALUES
    ipv4_frame.version = IPV4_VERSION;
    ipv4_frame.type = IPV4_TYPE;
    ipv4_frame.total_len = htons(ipv4_frame_len);
    ipv4_frame.id = htons(1);
    ipv4_frame.flags_offset = 0;
    ipv4_frame.TTL = IPV4_DEFAULT_TTL;
    ipv4_frame.protocol = protocol;
    ipv4_frame.checksum = IPV4_CHECKSUM_INIT;
    memcpy(ipv4_frame.source, layer->addr, sizeof(ipv4_addr_t));
    memcpy(ipv4_frame.dest, dst, sizeof(ipv4_addr_t));
    //multicast para ripv2
    ipv4_route_t multicast;
    memcpy(multicast.subnet_addr, IPv4_MULTICAST_ADDR, sizeof(ipv4_addr_t));
    memcpy(multicast.subnet_mask, IPv4_MULTICAST_NETWORK, sizeof(ipv4_addr_t));
    if (ipv4_route_lookup(&multicast, dst) == 4 ) ipv4_frame.TTL = 1;

    ipv4_frame.checksum = htons(ipv4_checksum((unsigned char *) &ipv4_frame, IPV4_HEADER_SIZE));

    memcpy(ipv4_frame.data, payload, payload_len);
    if (ipv4_route_lookup(&multicast, dst) != 4 ) {
        //Mandamos ARP resolve para conocer la MAC del siguiente salto
        //if (!is_broadcast) {
        if (arp_resolve(layer->iface, layer->addr, next_jump->gateway_addr, your_mac) <= 0) {
            //No hace falta mandar mensaje, ya lo hace arp_resolve
            return -1;
        }
    }
    else {
        memcpy(your_mac, MAC_MULTICAST_ADDR, sizeof(mac_addr_t));
    }

    int bytes_send = eth_send(layer->iface, your_mac, IPV4_PROTOCOL, (unsigned char *) &ipv4_frame,
                              ipv4_frame_len);
   
    return (bytes_send - IPV4_HEADER_SIZE);
}
//Para aceptar tambien multicast en ipv4_recv, simplemente comprobamos con mascaras de red binarias si esa dirección es multicast para aceptarla.
//224.0.0.0/4, es decir, el rango entre 224.0.0.0 y 239.255.255.255
int is_multicast(ipv4_addr_t addr) {
    int x = 1;

    for (int i; i < 4; i++) {
        if ((addr[i] & IPv4_MULTICAST_NETWORK[i]) != IPv4_MULTICAST_ADDR[i]) {
            return 0;
        }
    }
    return x;
}

int ipv4_recv(ipv4_layer_t *layer, uint8_t protocol, unsigned char buffer[], ipv4_addr_t sender, int buffer_len,
              long int timeout) {

    timerms_t timer;
    timerms_reset(&timer, timeout);

    mac_addr_t mac;

    int ipv4_buffer_len = buffer_len + IPV4_HEADER_SIZE;
    unsigned char ipv4_buffer[ipv4_buffer_len];
    ipv4_message_t *ipv4_frame = NULL;

    while (1) {

        //Temporizador

        long int time_left = timerms_left(&timer);

        //Hacemos un receive por la capa ethernet

        eth_recv(layer->iface, mac, IPV4_PROTOCOL, ipv4_buffer, ipv4_buffer_len, time_left);


        //Hacemos un cast a un struct ip
        ipv4_frame = (ipv4_message_t *) ipv4_buffer;

        //Confirmamos que el datagram IP sea del tipo correcto y el destino sea nuestra IP, tras esto se guardan los datos y se hace un break
        
        if (ipv4_frame->protocol == protocol && (memcmp(ipv4_frame->dest, layer->addr, sizeof(ipv4_addr_t)) == 0 ||
               is_multicast(ipv4_frame->dest) ) ) {

            break;
        }

    }

    memcpy(sender, ipv4_frame->source, sizeof(ipv4_addr_t));
    
    int payload_len = ntohs(ipv4_frame->total_len) - IPV4_HEADER_SIZE;
    if (buffer_len > payload_len) {
        buffer_len = payload_len;
    }
    memcpy(buffer, ipv4_frame->data, buffer_len);

    return payload_len;

}


int ipv4_close(ipv4_layer_t *ipv4_layer) {


    ipv4_route_table_free(ipv4_layer->routing_table);

    if (!eth_close(ipv4_layer->iface)) {
        return -1;
    }
    free(ipv4_layer);
    return 1;
}

/* void ipv4_addr_str ( ipv4_addr_t addr, char* str );
 *
 * DESCRIPCIÓN:
 *   Esta función genera una cadena de texto que representa la dirección IPv4
 *   indicada.
 *
 * PARÁMETROS:
 *   'addr': La dirección IP que se quiere representar textualente.
 *    'str': Memoria donde se desea almacenar la cadena de texto generada.
 *           Deben reservarse al menos 'IPv4_STR_MAX_LENGTH' bytes.
 */
void ipv4_addr_str(ipv4_addr_t addr, char *str) {
    if (str != NULL) {
        sprintf(str, "%d.%d.%d.%d",
                addr[0], addr[1], addr[2], addr[3]);
    }
}


/* int ipv4_str_addr ( char* str, ipv4_addr_t addr );
 *
 * DESCRIPCIÓN:
 *   Esta función analiza una cadena de texto en busca de una dirección IPv4.
 *
 * PARÁMETROS:
 *    'str': La cadena de texto que se desea procesar.
 *   'addr': Memoria donde se al6macena la dirección IPv4 encontrada.
 *
 * VALOR DEVUELTO:
 *   Se devuelve 0 si la cadena de texto representaba una dirección IPv4.
 *
 * ERRORES:
 *   La función devuelve -1 si la cadena de texto no representaba una
 *   dirección IPv4.
 */
int ipv4_str_addr(char *str, ipv4_addr_t addr) {
    int err = -1;

    if (str != NULL) {
        unsigned int addr_int[IPv4_ADDR_SIZE];
        int len = sscanf(str, "%d.%d.%d.%d",
                         &addr_int[0], &addr_int[1],
                         &addr_int[2], &addr_int[3]);

        if (len == IPv4_ADDR_SIZE) {
            int i;
            for (i = 0; i < IPv4_ADDR_SIZE; i++) {
                addr[i] = (unsigned char) addr_int[i];
            }

            err = 0;
        }
    }

    return err;
}


/*
 * uint16_t ipv4_checksum ( unsigned char * data, int len )
 *
 * DESCRIPCIÓN:
 *   Esta función calcula el checksum IP de los datos especificados.
 *
 * PARÁMETROS:
 *   'data': Puntero a los datos sobre los que se calcula el checksum.
 *    'len': Longitud en bytes de los datos.
 *
 * VALOR DEVUELTO:
 *   El valor del checksum calculado.
 */
uint16_t ipv4_checksum(unsigned char *data, int len) {
    int i;
    uint16_t word16;
    unsigned int sum = 0;

    /* Make 16 bit words out of every two adjacent 8 bit words in the packet
     * and add them up */
    for (i = 0; i < len; i = i + 2) {
        word16 = ((data[i] << 8) & 0xFF00) + (data[i + 1] & 0x00FF);
        sum = sum + (unsigned int) word16;
    }

    /* Take only 16 bits out of the 32 bit sum and add up the carries */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    /* One's complement the result */
    sum = ~sum;

    return (uint16_t) sum;
}

void ipv4_getAddr(ipv4_layer_t *layer, ipv4_addr_t addr) {
    if (layer != NULL) {
        memcpy(addr, layer->addr, sizeof(ipv4_addr_t));
    }
}
