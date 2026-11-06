/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef EMODBUS_H
#define EMODBUS_H
#ifdef MODBUS_BASE
#include "modbus.h"
#endif
#include <common/states.h>
#include <common/system/time.h>
#include <common/types.h>

#define MAX_HOST_SIZE        32
#define MAX_MODBUS_REGISTERS 32
#define DEFAULT_MODBUS_PORT  502
#define MAX_SENSORS          32

#ifndef MODBUS_BASE
typedef int modbus_t;
#endif

typedef struct emodbus_s {
    modbus_t *(*modbus_new_tcp)(const char *ip, int port);
    int (*modbus_set_slave)(modbus_t *ctx, int slave);
    int (*modbus_connect)(modbus_t *ctx);
    void (*modbus_close)(modbus_t *ctx);
    void (*modbus_free)(modbus_t *ctx);
    int (*modbus_read_registers)(modbus_t *ctx, int addr, int nb, uint16_t *dest);
    int (*modbus_write_registers)(modbus_t *ctx, int addr, int nb, const uint16_t *src);
    const char *(*modbus_strerror)(int errnum);
} emodbus_t;

typedef struct modbus_reg_s {
    char *name;
    uint reg; // Register address
} modbus_reg_t;

typedef struct emodbus_dev_s {
    char ip[MAX_HOST_SIZE];
    int port;
    uint slave_id;
    modbus_t *ctx;
    modbus_reg_t registers[MAX_MODBUS_REGISTERS];
    int num_registers;
    uint reg_consecutive; // If sensors registers are consecutive, they can be read at once
    timestamp_t time;
} emodbus_dev_t;

state_t emodbus_get_info(); // TODO: device info can be read from some registers
state_t emodbus_get_devices(emodbus_dev_t **devs, uint *devs_count);
state_t emodbus_open(emodbus_t *modbus_in);
state_t emodbus_dev_init(emodbus_dev_t *dev, char ip[], int port, uint slave_id, modbus_reg_t registers[],
                         int num_registers, uint reg_consecutive);
state_t emodbus_read_all(emodbus_dev_t *dev, uint16_t *data);
state_t emodbus_read(emodbus_dev_t *dev, int register_address_idx, uint16_t *data);
state_t emodbus_write_all(emodbus_dev_t *dev, uint16_t *data);
state_t emodbus_write(emodbus_dev_t *dev, int register_address_idx, uint16_t data);
state_t emodbus_close(emodbus_dev_t *dev);
char *emodbus_dev_tostr(emodbus_dev_t *dev, char *buffer, int *len);
int emodbus_is_privileged();
state_t parse_register_list(char *register_list, modbus_reg_t registers[], int *num_registers, uint *reg_consecutive);

#endif // EMODBUS_H