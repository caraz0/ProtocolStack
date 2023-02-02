#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <rawnet.h>

#include "ripv2_route_table.h"
#include "arp.h"


int main(int argc, char *argv[]) {

    ipv4_addr_t sender_ip;
    uint16_t port;
    ripv2_msg_t buffer;
    int min_time, bytes, n_entries, index;


    if ((argc <= 3) || (argc > 4)) {
        printf("       <string.txt>: IPV4_config.txt\n");
        printf("       <string.txt>: IPV4_route_table.txt\n");
        printf("       <string.txt>: RIPv2_route_table.txt\n");
        exit(-1);
    }

    char *config_name = argv[1];
    char *route_table_name = argv[2];
    char *rip_route_table_name = argv[3];

    rip_route_table_t *table = ripv2_route_table_create();
    timers_t *timers = malloc(sizeof(timers_t));

    int last_index = ripv2_route_table_read(rip_route_table_name, table);

    ripv2_inicialize_timers(last_index, timers);

    ripv2_route_table_output_with_timers(table, timers);

    udp_layer_t *udp_layer = udp_open(RIP_PORT, config_name, route_table_name);
   
    while (1) {



        min_time = ripv2_timeleft(table, *timers);

        printf("El timer mas bajo es %i\n", min_time);

        bytes = udp_recv(udp_layer, min_time, sender_ip, &port, (unsigned char *) &buffer, sizeof(buffer));

        ripv2_route_table_remove_expired(table, timers); 
        ripv2_route_table_write(table, rip_route_table_name);

        if (bytes <= 0) continue;

        n_entries = (bytes - RIP_HEADER_SIZE) / sizeof(entrada_rip_t);

        if (port != RIP_PORT || n_entries == 0) continue; 

        if (buffer.type == RIPv2_REQUEST) {

            printf("Request recibido\n");

            ripv2_msg_t msg;
            msg.type = RIPv2_RESPONSE;
            msg.version = RIPv2_TYPE_VERSION;
            index = 0;


            if (n_entries == 1 && buffer.entrada[0].family_directions == 0 &&
                ripv2_is_infinite(ntohl(buffer.entrada[0].metric))) {

                
                for (int i = 0; i < RIP_ROUTE_TABLE_SIZE; i++) {
                    entrada_rip_t *entry = table->routes[i];

                    if (entry != NULL) {
                        msg.entrada[index] = *(entry);
                        msg.entrada[index].family_directions = htons(msg.entrada[index].family_directions);
                        msg.entrada[index].metric = htonl(msg.entrada[index].metric);
                        index++;
                    }
                }



            } else { 



                for (int i = 0; i < n_entries; i++) {
                    entrada_rip_t entry = buffer.entrada[i];

                    int aux = ripv2_route_table_find(table, &entry);
                    if (aux < 0) entry.metric = htonl(16);
                    else entry.metric = htonl(table->routes[aux]->metric);
                    msg.entrada[index] = entry;
                    index++;

                }
            }

           

            udp_send(udp_layer, sender_ip, RIP_PORT, (unsigned char *) &msg,
                     sizeof(entrada_rip_t) * index + RIP_HEADER_SIZE);

        } else if (buffer.type == RIPv2_RESPONSE) { 

            printf("Response recibido\n");
            printf("Rutas Recibidas: %i\n", n_entries);
            entrada_rip_t entry;

            for (int i = 0; i < n_entries; i++) {
                
                entry = buffer.entrada[i];
                if (ripv2_is_infinite(ntohl(entry.metric))) continue;

                entry.metric = entry.metric + htonl(1);

                int aux = ripv2_route_table_find(table, &entry);

                if (aux < 0 && !ripv2_is_infinite(ntohl(entry.metric))) { 
                    
                    memcpy(entry.gw, sender_ip, sizeof(ipv4_addr_t));
                    entry.metric = ntohl(entry.metric);
                    entry.family_directions = ntohl(entry.family_directions);

                    entrada_rip_t *entrada_aux = malloc(sizeof(entrada_rip_t));
                    memcpy(entrada_aux, &entry, sizeof(entrada_rip_t));
                    int added_index = ripv2_route_table_add(table, entrada_aux);
                    timerms_reset(&(timers->list_timers[added_index]), RIP_ROUTE_DEFAULT_TIME);

                } else if (aux >= 0) { 
                    entrada_rip_t *entry_on_table = ripv2_route_table_get(table, aux);

                    if (memcmp(entry_on_table->gw, sender_ip, sizeof(ipv4_addr_t)) == 0) {
                        timerms_reset(&(timers->list_timers[aux]), RIP_ROUTE_DEFAULT_TIME);
                        if (entry_on_table->metric != ntohl(entry.metric)) {
                            entry_on_table->metric = ntohl(entry.metric);
                        }
                    }

                    else if (ntohl(entry.metric) < entry_on_table->metric)  {
                            memcpy(entry_on_table->gw, sender_ip, sizeof(ipv4_addr_t));
                        }

                    }

                }

            }

        ripv2_route_table_output_with_timers(table, timers);
        ripv2_route_table_write(table, rip_route_table_name);

    }

}
