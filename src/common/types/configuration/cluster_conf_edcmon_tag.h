/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef _EDCMONTAG_CONF_H
#define _EDCMONTAG_CONF_H

#include <common/config.h>
#include <common/states.h>
#include <common/types/generic.h>

typedef enum {
    storage     = 0,
    network     = 1,
    management  = 2,
    ads_chiller = 3,
    two_phase   = 4,
    others      = 5
} pdu_t;

#define MAX_HOSTS 32

#define pdu_type_to_str(t)                                                                                             \
    ((t == storage)                                                                                                    \
         ? "storage"                                                                                                   \
         : ((t == network)                                                                                             \
                ? "network"                                                                                            \
                : ((t == management)                                                                                   \
                       ? "management"                                                                                  \
                       : ((t == ads_chiller) ? "ads_chiller" : ((t == two_phase) ? "two_phase" : "others")))))

typedef struct edcmon {
    char id[GENERIC_NAME];
    char host[GENERIC_NAME];
    int port;
    pdu_t type;
    char sensors_list[1024];
    char pdu_list[1024];
    int eargm_id;
} edcmon_t;

state_t EDCMON_token(char *token);
state_t EDCMON_parse_token(edcmon_t **edcmon_i, unsigned int *num_edcmon_i, char *line);
void print_edcmon_tags_conf(edcmon_t *edcmon_tag, uint i);
state_t parse_host_list(char *host_list, char **hosts, int *ports, int *num_hosts);
state_t parse_ip_port(char *string, char *ip, int *port);

#endif
