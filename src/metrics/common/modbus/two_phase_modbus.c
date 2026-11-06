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
#include <common/output/debug.h>
#include <common/states.h>
#include <common/system/monitor.h>
#include <data_center_monitor/plugins/conf.h>
#include <metrics/common/modbus/emodbus.h>
#include <metrics/common/modbus/two_phase_modbus.h>
#include <metrics/two_phase/two_phase.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static emodbus_t modbus;
suscription_t *two_phase_sus;
static uint16_t metric_register_data[TWO_PHASE_MAX_METRIC + 1];
two_phase_t *two_phase_pool_registers;

typedef struct two_phase_modbus_dev_s {
    emodbus_dev_t dev;
    int working;
} two_phase_modbus_dev_t;

static two_phase_modbus_dev_t *pool;
static two_phase_metric_type **register_to_enum;
static int **enum_to_register;
uint two_phase_ok_pool;
uint two_phase_modbus_devs_count;
uint two_phase_initialized = 0;
static conf_t *conf;

static void load_atfork()
{
    // Just lock control is enough
    pthread_mutex_unlock(&lock);
}

void base_two_phase_modbus_load(two_phase_ops_t *ops, int force_api)
{
    // TODO: I am not sure wether root privileges are needed here, and this two_phase_ok_pool thing.
    debug("Received API %d", force_api);
    if (API_IS(force_api, API_DEFAULT)) {
        two_phase_ok_pool = 1;
    } else if (API_IS(force_api, API_FREE)) {
        two_phase_ok_pool = emodbus_is_privileged();
    }
    debug("Pooling %d", two_phase_ok_pool);
    // The locking and duplicate control is at higher level
    if (state_fail(emodbus_open(&modbus))) {
        debug("modbus_open failed: %s", state_msg);
        return;
    }

    // Atfork control
    pthread_atfork(NULL, NULL, load_atfork);
    apis_set(ops->get_info, two_phase_modbus_get_info);
    apis_set(ops->get_devices, two_phase_modbus_get_devices);
    // apis_set(ops->init, two_phase_modbus_init);
    apis_set(ops->dispose, two_phase_modbus_dispose);
    // apis_set(ops->read, two_phase_modbus_read);
    // apis_set(ops->read_raw, two_phase_modbus_read_raw);
    apis_set(ops->get_metric_mult, two_phase_modbus_get_metric_mult);
    apis_set(ops->get_is_kelvin, two_phase_modbus_get_is_kelvin);

    debug("Loaded MODBUS");
}

// TODO: Get info but it is setting info
void two_phase_modbus_get_info(apinfo_t *info)
{
    info->api        = API_MODBUS;
    info->devs_count = two_phase_modbus_devs_count;
}

void two_phase_modbus_get_devices(two_phase_devs_t **devs_in, uint *devs_count_in)
{
    // int r;
    int i;

    if (devs_in != NULL) {
        *devs_in = calloc(two_phase_modbus_devs_count, sizeof(two_phase_devs_t));
        //
        for (i = 0; i < two_phase_modbus_devs_count; ++i) {
            // TODO: I don't have a get_serial function yet, it might be implemented
            // using modbus read on a specific register
            // if ((r = modbus.get_serial(devs[i], serial, 32)) != MODBUS_SUCCESS) {
            //     return_msg(, "MODBUS error");
            // }
            // (*devs_in)[i].serial = (ullong) atoll(serial);
            (*devs_in)[i].serial = i;
            (*devs_in)[i].index  = i;
        }
    }
    if (devs_count_in != NULL) {
        *devs_count_in = two_phase_modbus_devs_count;
    }
}

/**
 * Maps register names to their corresponding metric enumeration for a specific device.
 *
 * @param id The device identifier used to index into the register_to_enum array
 * @param registers Array of Modbus registers to map
 * @param num_registers Number of registers in the array
 *
 * This function populates the register_to_enum mapping for a specific device,
 * converting register names from ear.conf to their corresponding metric enumeration values
 * defined in the code. This mapping allows devices with different register configurations.
 */
static void map_registers(int id, modbus_reg_t *registers, int num_registers)
{
    for (int i = 0; i < num_registers; i++) {
        register_to_enum[id][i]                       = two_phase_get_metric_enum(registers[i].name);
        enum_to_register[id][register_to_enum[id][i]] = i;
    }
}

state_t base_two_phase_modbus_init(ctx_t *c)
{
    state_t s = EAR_SUCCESS;
    conf      = (conf_t *) c->context;
    timestamp_t time;
    int j = 0;
    modbus_reg_t registers[MAX_MODBUS_REGISTERS];
    int num_registers;
    int port;
    uint slave_id = 1; // Slave ID is 1 by default
    uint reg_consecutive;
    int total_chillers_count = 0;
    char hostname[256];
    char my_hostname[256];

    if (two_phase_initialized) {
        return s;
    }

    two_phase_modbus_devs_count = 0;

    if (gethostname(my_hostname, sizeof(my_hostname)) != 0) {
        debug("Failed to get hostname");
        return EAR_ERROR;
    }

    // static_init();
    timestamp_getfast(&time);
    if (conf->cluster.num_edcmons_tags) {
        for (uint i = 0; i < conf->cluster.num_edcmons_tags; i++) {
            if (conf->cluster.edcmon_tags[i].type == two_phase) {
                // Consider the device only if controller hostname matches
                if (strcmp(my_hostname, conf->cluster.edcmon_tags[i].host) == 0) {
                    two_phase_modbus_devs_count++;
                }
                total_chillers_count++;
            }
        }
        pool = (two_phase_modbus_dev_t *) malloc(two_phase_modbus_devs_count * sizeof(two_phase_modbus_dev_t));
        if (pool == NULL) {
            debug("Memory allocation failed for pool");
            return EAR_ERROR;
        }
        two_phase_pool_registers = calloc(two_phase_modbus_devs_count, sizeof(two_phase_t));
        register_to_enum =
            (two_phase_metric_type **) malloc(two_phase_modbus_devs_count * sizeof(two_phase_metric_type *));
        enum_to_register = (int **) malloc(two_phase_modbus_devs_count * sizeof(int *));
        if (!two_phase_pool_registers || !register_to_enum || !enum_to_register) {
            debug("Memory allocation failed");
            return EAR_ERROR;
        }

        for (int i = 0; i < two_phase_modbus_devs_count; i++) {
            register_to_enum[i] =
                (two_phase_metric_type *) malloc(TWO_PHASE_MAX_METRIC * sizeof(two_phase_metric_type));
            enum_to_register[i] = (int *) malloc(TWO_PHASE_MAX_METRIC * sizeof(int));
            for (int j = 0; j < TWO_PHASE_MAX_METRIC; j++) {
                register_to_enum[i][j] = TWO_PHASE_UNKNOWN_METRIC;
                enum_to_register[i][j] = -1;
            }
        }

        for (uint i = 0; i < conf->cluster.num_edcmons_tags; i++) {
            if (conf->cluster.edcmon_tags[i].type == two_phase) {
                // pool[i].correct = 1;

                if (strcmp(my_hostname, conf->cluster.edcmon_tags[i].host) != 0) {
                    debug("Current host %s not in host_list, skipping device", my_hostname);
                    continue;
                }

                // Format 1: all key=value pairs
                // char *list1 = "temp:40,power:41,current:42";

                // Format 2: first key=value followed by keys with incremental registers
                // char *list2 = "temp:40,power,current,voltage";
                parse_register_list(conf->cluster.edcmon_tags[i].sensors_list, registers, &num_registers,
                                    &reg_consecutive);

                if (num_registers <= 0) {
                    // use default registers configuration
                    for (int r = 0; r < TWO_PHASE_MAX_METRIC; r++) {
                        registers[r].name = strdup(two_phase_metric_str(r));
                        registers[r].reg  = r;
                    }
                    num_registers   = TWO_PHASE_MAX_METRIC;
                    reg_consecutive = 1;
                }

                // One host per device in the format host:port
                parse_ip_port(conf->cluster.edcmon_tags[i].pdu_list, hostname, &port);
                if (port == -1)
                    port = DEFAULT_MODBUS_PORT;
                debug("Device IP: %s,  port: %d", hostname, port);
                if (state_fail(emodbus_dev_init(&pool[j].dev, hostname, port, slave_id, registers, num_registers,
                                                reg_consecutive))) {
                    debug("emodbus_dev_init failed, skipping device");
                    two_phase_modbus_devs_count--;
                    free(register_to_enum[two_phase_modbus_devs_count]);
                    free(enum_to_register[two_phase_modbus_devs_count]);
                    continue;
                }
                debug("Num registers: %d", num_registers);
                for (int r = 0; r < num_registers; r++) {
                    debug("%s:%d", registers[r].name, registers[r].reg);
                }
                map_registers(j, registers, num_registers);

                pool[j].dev.time = time;
                j++;

                // free registers
                for (int k = 0; k < num_registers; k++) {
                    free(registers[k].name);
                }
            }
        }
        if (!j) {
            debug("No devices found");
            // s = EAR_ERROR; Will still return EAR_SUCCESS
        }
        s                     = EAR_SUCCESS;
        two_phase_initialized = 1;
    }

    return s;
}

state_t two_phase_modbus_dispose(ctx_t *c)
{
    if (two_phase_initialized) {
        if (two_phase_sus != NULL) {
            monitor_unregister(two_phase_sus);
        }
        pthread_mutex_lock(&lock);
        two_phase_initialized = 0;
        for (int i = 0; i < two_phase_modbus_devs_count; i++) {
            emodbus_close(&pool[i].dev);
            for (int j = 0; j < pool[i].dev.num_registers; j++) {
                free(pool[i].dev.registers[j].name);
            }
            free(register_to_enum[i]);
            free(enum_to_register[i]);
        }

        free(register_to_enum);
        free(enum_to_register);
        free(pool);
        free(two_phase_pool_registers);
        two_phase_modbus_devs_count = 0;

        pthread_mutex_unlock(&lock);
    }
    return EAR_SUCCESS;
}

static int is_working(int i)
{
    // TODO
    return 1;
}

state_t two_phase_modbus_static_read(int i, two_phase_t *metric)
{
    int s;

    memset(metric_register_data, 0, sizeof(uint16_t) * (TWO_PHASE_MAX_METRIC + 1));

    s = emodbus_read_all(&pool[i].dev, metric_register_data);

    // Pooling the data (time is not set here)
    for (int r = 0; r < pool[i].dev.num_registers; ++r) {
        metric->register_data[r] = (uint32_t) metric_register_data[r];
    }
    metric->samples       = 1;
    metric->num_registers = pool[i].dev.num_registers;
    metric->working       = is_working(i);
    // In the future we have to check if there is any error in any returned status.
    // metric->correct  = 1;
    //
    return s;
}

void two_phase_modbus_get_value(two_phase_metric_type metric, int dev_idx, uint16_t *value)
{
    if (metric >= TWO_PHASE_MAX_METRIC) {
        *value = -1;
        return;
    }
    emodbus_read(&pool[dev_idx].dev, enum_to_register[dev_idx][metric], value);

    two_phase_pool_registers[dev_idx].register_data[enum_to_register[dev_idx][metric]] = *value;
}

void two_phase_modbus_get_value_all(two_phase_metric_type metric, uint16_t *values)
{
    if (metric >= TWO_PHASE_MAX_METRIC) {
        for (int i = 0; i < two_phase_modbus_devs_count; i++) {
            values[i] = -1;
        }
        return;
    }
    for (int i = 0; i < two_phase_modbus_devs_count; i++) {
        emodbus_read(&pool[i].dev, enum_to_register[i][metric], &values[i]);
        two_phase_pool_registers[i].register_data[enum_to_register[i][metric]] = values[i];
    }
}

state_t two_phase_modbus_set_value(ctx_t *c, two_phase_metric_type metric, int dev_idx, double value)
{
    uint16_t reg_value;
    if (metric >= TWO_PHASE_MAX_METRIC || dev_idx < 0 || dev_idx >= two_phase_modbus_devs_count) {
        return EAR_ERROR;
    }

    reg_value = (uint16_t) value;

    if (state_fail(emodbus_write(&pool[dev_idx].dev, enum_to_register[dev_idx][metric], reg_value))) {
        return EAR_ERROR;
    }

    two_phase_pool_registers[dev_idx].register_data[enum_to_register[dev_idx][metric]] = (uint32_t) reg_value;
    return EAR_SUCCESS;
}

state_t two_phase_modbus_set_value_all(ctx_t *c, two_phase_metric_type metric, const double *values)
{
    if (metric >= TWO_PHASE_MAX_METRIC) {
        return EAR_ERROR;
    }

    for (int i = 0; i < two_phase_modbus_devs_count; i++) {
        if (state_fail(two_phase_modbus_set_value(c, metric, i, values[i]))) {
            error("Failed to write value to device %d", i);
        }
    }
    return EAR_SUCCESS;
}

void two_phase_reorder_metrics(two_phase_t *data, int i)
{
    for (int r = 0; r < two_phase_pool_registers[i].num_registers; ++r) {
        // two_phase_pool_registers[i].register_data[register_to_enum[i][r]] += metric.register_data[r];
        data[i].register_data[register_to_enum[i][r]] = two_phase_pool_registers[i].register_data[r];
        debug("Device %d, register %d: %s = %d", i, r, two_phase_metric_str(register_to_enum[i][r]),
              data[i].register_data[register_to_enum[i][r]]);
    }
}

state_t two_phase_modbus_read_raw(ctx_t *c, two_phase_t *data)
{
    timestamp_t time;
    int i;
    timestamp_getfast(&time);
    for (i = 0; i < two_phase_modbus_devs_count; ++i) {
        two_phase_modbus_static_read(
            i, &data[i]); // WARNING: Data is not correctly sorted here! enum will not match metrics
        data[i].time = time;
    }
    return EAR_SUCCESS;
}

// Actually you might need this only in the case of read_raw
// uint16_t two_phase_get_metric(two_phase_t *data, two_phase_metric_type metric)
// {
//     if (metric >= TWO_PHASE_MAX_METRIC) {
//         return -1;
//     }
//     return data->register_data[register_to_enum[i][metric]];
// }

float two_phase_modbus_get_metric_mult()
{
    return (float) METRIC_MULT;
}

int two_phase_modbus_get_is_kelvin()
{
    return TEMP_KELVIN;
}