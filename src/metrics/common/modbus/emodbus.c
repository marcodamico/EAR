/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

// #define SHOW_DEBUGS 1
#include <pthread.h>
#ifdef MODBUS_BASE
#include <modbus.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include <common/config/config_env.h>
#include <common/output/debug.h>
#include <common/states.h>
#include <common/system/symplug.h>
#include <common/types.h>
// #include <common/output/verbose.h>
#include <common/states.h>

#include <metrics/common/modbus/emodbus.h>

#define HACK_MODBUS_FILE "HACK_MODBUS_FILE"

#ifndef MODBUS_BASE
#define MODBUS_PATH ""
#define MODBUS_N    0
#else
#define MODBUS_PATH MODBUS_BASE
#define MODBUS_N    8
#endif

#define MODBUS_SUCCESS 0

static const char *modbus_names[] = {"modbus_new_tcp",         "modbus_set_slave", "modbus_connect",
                                     "modbus_close",           "modbus_free",      "modbus_read_registers",
                                     "modbus_write_registers", "modbus_strerror"};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static emodbus_t modbus;
static uint ok;

static state_t static_open()
{
#define open_test(path)                                                                                                \
    debug("Opening %s", path);                                                                                         \
    if (state_ok(plug_open(path, (void **) &modbus, modbus_names, MODBUS_N, RTLD_NOW | RTLD_LOCAL))) {                 \
        return EAR_SUCCESS;                                                                                            \
    }

#define build_path_and_test(base_path)                                                                                 \
    if (base_path) {                                                                                                   \
        char path[MAX_PATH_SIZE];                                                                                      \
        strncpy(path, base_path, sizeof(path));                                                                        \
        strncat(path, "/lib64/libmodbus.so", sizeof(path) - strlen(path) - 1);                                         \
        open_test(path);                                                                                               \
        strncat(path, ".5", sizeof(path) - strlen(path) - 1);                                                          \
        open_test(path);                                                                                               \
    }

    // Looking for Modbus library in typical paths.
    open_test(ear_getenv(HACK_MODBUS_FILE));

    char *modbus_root = ear_getenv("MODBUS_ROOT");
    char *modbus_home = ear_getenv("MODBUS_HOME");

    build_path_and_test(modbus_root);
    build_path_and_test(modbus_home);

    open_test(MODBUS_PATH "/targets/x86_64-linux/lib/libmodbus.so");
    open_test(MODBUS_PATH "/targets/x86_64-linux/lib/libmodbus.so.5");
    open_test(MODBUS_PATH "/lib64/libmodbus.so");
    open_test(MODBUS_PATH "/lib64/libmodbus.so.5");
    open_test(MODBUS_PATH "/lib/libmodbus.so");
    open_test(MODBUS_PATH "/lib/libmodbus.so.5");
    open_test("/usr/lib64/libmodbus.so");
    open_test("/usr/lib64/libmodbus.so.5");
    open_test("/usr/lib/x86_64-linux-gnu/libmodbus.so");
    open_test("/usr/lib/x86_64-linux-gnu/libmodbus.so.5");
    open_test("/usr/lib32/libmodbus.so");
    open_test("/usr/lib32/libmodbus.so.5");
    open_test("/usr/lib/libmodbus.so");
    open_test("/usr/lib/libmodbus.so.5");
    open_test("/.singularity.d/libs/libmodbus.so");
    open_test("/.singularity.d/libs/libmodbus.so.5");
    open_test(EAR_INSTALL_PATH "/lib/deps/libmodbus.so");
    open_test(EAR_INSTALL_PATH "/lib/deps/libmodbus.so.5");
    return_msg(EAR_ERROR, "Can not load libmodbus.so");
}

static state_t static_init()
{
    return EAR_SUCCESS;
}

static state_t static_free(state_t s, char *error)
{
    return_msg(s, error);
}

state_t emodbus_open(emodbus_t *modbus_in)
{
    state_t s = EAR_SUCCESS;
#ifndef MODBUS_BASE
    debug("No MODBUS_BASE path provided");
    return EAR_ERROR;
#endif
    while (pthread_mutex_trylock(&lock))
        ;
    if (ok) {
        goto fini;
    }
    if (state_ok(s = static_open())) {
        if (state_ok(s = static_init())) {
            ok = 1;
        }
    }

fini:
    pthread_mutex_unlock(&lock);
    if (modbus_in != NULL) {
        memcpy(modbus_in, &modbus, sizeof(emodbus_t));
    }
    debug("Initialized Modbus");
    return EAR_SUCCESS;
}

state_t emodbus_dev_init(emodbus_dev_t *dev, char ip[], int port, uint slave_id, modbus_reg_t registers[],
                         int num_registers, uint reg_consecutive)
{
    dev->ctx = modbus.modbus_new_tcp(ip, port);
    if (dev->ctx == NULL) {
        error("Failed to create modbus context");
        return EAR_ERROR;
    }

    if (slave_id > 0 && modbus.modbus_set_slave(dev->ctx, slave_id) == -1) {
        error("Invalid slave ID");
        modbus.modbus_free(dev->ctx);
        return EAR_ERROR;
    }

    if (modbus.modbus_connect(dev->ctx) == -1) {
        error("Connection failed: %s\n", modbus.modbus_strerror(errno));
        modbus.modbus_free(dev->ctx);
        return EAR_ERROR;
    }

    strcpy(dev->ip, ip);
    dev->port     = port;
    dev->slave_id = slave_id;
    for (int i = 0; i < num_registers; i++) {
        dev->registers[i].reg  = registers[i].reg;
        dev->registers[i].name = strdup(registers[i].name);
    }
    dev->num_registers   = num_registers;
    dev->reg_consecutive = reg_consecutive;

    return EAR_SUCCESS;
}

state_t emodbus_read_all(emodbus_dev_t *dev, uint16_t *data)
{
    int rc;
    if (dev->reg_consecutive) {
        rc = modbus.modbus_read_registers(dev->ctx, dev->registers[0].reg, dev->num_registers, data);
        if (rc == -1) {
            error("Reading registers: %s\n", modbus.modbus_strerror(errno));
            return EAR_ERROR;
        }
    } else {
        // Sparse register map: one request per register, stored by its position in the list.
        for (int i = 0; i < dev->num_registers; i++) {
            rc = modbus.modbus_read_registers(dev->ctx, dev->registers[i].reg, 1, &data[i]);
            if (rc == -1) {
                error("Reading register %s (%d): %s\n", dev->registers[i].name, dev->registers[i].reg,
                      modbus.modbus_strerror(errno));
                return EAR_ERROR;
            }
        }
    }
    for (int i = 0; i < dev->num_registers; i++) {
        debug("Register %s (%d) = %d", dev->registers[i].name, dev->registers[i].reg, data[i]);
    }
    timestamp_getfast(&dev->time);
    return EAR_SUCCESS;
}

state_t emodbus_read(emodbus_dev_t *dev, int register_address_idx, uint16_t *data)
{
    int rc;
    // I am not sure this will work, is all data the same size? TODO
    rc = modbus.modbus_read_registers(dev->ctx, dev->registers[register_address_idx].reg, 1, data);
    if (rc == -1) {
        error("Error reading registers: %s\n", modbus.modbus_strerror(errno));
        // emodbus_close(dev);
        return EAR_ERROR;
    }
    timestamp_getfast(&dev->time);
    return EAR_SUCCESS;
}

// state_t emodbus_write_all(emodbus_dev_t *dev, uint16_t *data)
// {
//     int rc;
//     rc = modbus.modbus_write_registers(dev->ctx, dev->registers[0].reg, dev->num_registers, data);
//     if (rc == -1) {
//         error("Error writing registers: %s\n", modbus.modbus_strerror(errno));
//         emodbus_close(dev);
//         return EAR_ERROR;
//     }
//     // timestamp_getfast(&dev->time);
//     return EAR_SUCCESS;
// }

state_t emodbus_write(emodbus_dev_t *dev, int register_address_idx, uint16_t data)
{
    int rc;
    rc = modbus.modbus_write_registers(dev->ctx, dev->registers[register_address_idx].reg, 1, &data);
    if (rc == -1) {
        error("Error writing registers: %s\n", modbus.modbus_strerror(errno));
        // emodbus_close(dev);
        return EAR_ERROR;
    }
    // timestamp_getfast(&dev->time);
    return EAR_SUCCESS;
}

state_t emodbus_close(emodbus_dev_t *dev)
{
    modbus.modbus_close(dev->ctx);
    modbus.modbus_free(dev->ctx);
    dev->ctx = NULL;

    return EAR_SUCCESS;
}

int emodbus_is_privileged()
{
    // Provisional
    return (getuid() == 0);
}

char *emodbus_dev_tostr(emodbus_dev_t *dev, char *buffer, int *len)
{
    int i;
    int l   = 0;
    char *p = buffer;

    for (i = 0; i < dev->num_registers; i++) {
        int written = snprintf(p, MAX_PATH_SIZE - l, "%s=%d", dev->registers[i].name, dev->registers[i].reg);
        p += written;
        l += written;

        if (i < dev->num_registers - 1) {
            written = snprintf(p, MAX_PATH_SIZE - l, ",");
            p += written;
            l += written;
        }
    }
    *len = l;
    return buffer;
}

state_t parse_register_list(char *register_list, modbus_reg_t registers[], int *num_registers, uint *reg_consecutive)
{
    char *token;
    char *saveptr;
    char *key;
    char *value;
    int count        = 0;
    int base_reg     = -1;
    *reg_consecutive = 0;

    token = strtok_r(register_list, ",", &saveptr);
    while (token != NULL && count < MAX_SENSORS) {
        // Remove whitespace
        while (isspace(*token))
            token++;

        // Check if token contains ":"
        if ((value = strchr(token, ':')) != NULL) {
            *value = '\0';
            value++;
            key                   = token;
            registers[count].name = strdup(key);
            registers[count].reg  = atoi(value);
            base_reg              = registers[count].reg;
        } else {
            // No ":", use incremental register
            *reg_consecutive      = 1;
            registers[count].name = strdup(token);
            registers[count].reg  = ++base_reg;
        }
        count++;
        token = strtok_r(NULL, ",", &saveptr);
    }

    // A single read request is only valid if the addresses really are consecutive, whatever
    // format the list was written in (a "name:reg" entry may break the sequence).
    *reg_consecutive = 1;
    for (int i = 1; i < count; i++) {
        if (registers[i].reg != registers[0].reg + i) {
            *reg_consecutive = 0;
            break;
        }
    }

    *num_registers = count;
    return EAR_SUCCESS;
}
