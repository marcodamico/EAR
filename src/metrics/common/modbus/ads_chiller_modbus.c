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
#include <common/system/monitor.h>
#include <data_center_monitor/plugins/conf.h>
#include <metrics/ads_chiller/ads_chiller.h>
#include <metrics/common/modbus/ads_chiller_modbus.h>
#include <metrics/common/modbus/emodbus.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static emodbus_t modbus;
suscription_t *ads_chiller_sus;
static uint16_t metric_register_data[ADS_CHILLER_MAX_METRIC + 1];
ads_chiller_t *ads_chiller_pool_registers;

typedef struct ads_chiller_modbus_dev_s {
    emodbus_dev_t dev;
} ads_chiller_modbus_dev_t;

static ads_chiller_modbus_dev_t *pool;
static ads_chiller_metric_type **register_to_enum;
static int **enum_to_register;
uint ads_chiller_ok_pool;
uint ads_chiller_modbus_devs_count;
uint ads_chiller_initialized;
static uint ads_chiller_modbus_loaded = 0;
static conf_t *conf;
// A copy, not a pointer to the caller's table. Holding a pointer meant the already-loaded path
// copied the struct onto itself whenever the caller reused or cleared the same ops variable, and
// handed back a table of NULLs.
static ads_chiller_ops_t chiller_ops;

static void load_atfork()
{
    // Just lock control is enough
    pthread_mutex_unlock(&lock);
}

void base_ads_chiller_modbus_load(ads_chiller_ops_t *ops, int force_api)
{
    // TODO: I am not sure wether root privileges are needed here, and this ads_chiller_ok_pool thing.
    if (ads_chiller_modbus_loaded) {
        debug("Already loaded");
        *ops = chiller_ops;
        return;
    }
    debug("Received API %d", force_api);
    if (API_IS(force_api, API_DEFAULT)) {
        ads_chiller_ok_pool = 1;
    } else if (API_IS(force_api, API_FREE)) {
        ads_chiller_ok_pool = emodbus_is_privileged();
    }
    debug("Pooling %d", ads_chiller_ok_pool);
    // The locking and duplicate control is at higher level
    if (state_fail(emodbus_open(&modbus))) {
        debug("modbus_open failed: %s", state_msg);
        return;
    }

    // Atfork control
    pthread_atfork(NULL, NULL, load_atfork);
    apis_set(ops->get_info, ads_chiller_modbus_get_info);
    apis_set(ops->get_devices, ads_chiller_modbus_get_devices);
    // apis_set(ops->init, ads_chiller_modbus_init); Init is metrics/management specific
    apis_set(ops->dispose, ads_chiller_modbus_dispose);
    // apis_set(ops->read, ads_chiller_modbus_read);
    // apis_set(ops->read_raw, ads_chiller_modbus_read_raw);
    apis_set(ops->get_metric_mult, ads_chiller_modbus_get_metric_mult);
    apis_set(ops->get_is_kelvin, ads_chiller_modbus_get_is_kelvin);

    ads_chiller_modbus_loaded = 1;
    chiller_ops               = *ops;
    debug("Loaded MODBUS");
}

// TODO: Get info but it is setting info
void ads_chiller_modbus_get_info(apinfo_t *info)
{
    info->api        = API_MODBUS;
    info->devs_count = ads_chiller_modbus_devs_count;
}

void ads_chiller_modbus_get_devices(ads_chiller_devs_t **devs_in, uint *devs_count_in)
{
    // int r;
    int i;

    if (devs_in != NULL) {
        *devs_in = calloc(ads_chiller_modbus_devs_count, sizeof(ads_chiller_devs_t));
        //
        for (i = 0; i < ads_chiller_modbus_devs_count; ++i) {
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
        *devs_count_in = ads_chiller_modbus_devs_count;
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
        register_to_enum[id][i]                       = ads_chiller_get_metric_enum(registers[i].name);
        enum_to_register[id][register_to_enum[id][i]] = i;
    }
}

state_t base_ads_chiller_modbus_init(ctx_t *c)
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

    if (ads_chiller_initialized) {
        debug("Already initialized");
        return s;
    }

    ads_chiller_modbus_devs_count = 0;

    if (gethostname(my_hostname, sizeof(my_hostname)) != 0) {
        debug("Failed to get hostname");
        return EAR_ERROR;
    }

    // static_init();
    timestamp_getfast(&time);
    if (conf->cluster.num_edcmons_tags) {
        for (uint i = 0; i < conf->cluster.num_edcmons_tags; i++) {
            if (conf->cluster.edcmon_tags[i].type == ads_chiller) {
                // Consider the device only if controller hostname matches
                if (strcmp(my_hostname, conf->cluster.edcmon_tags[i].host) == 0) {
                    ads_chiller_modbus_devs_count++;
                }
                total_chillers_count++;
            }
        }
        pool = (ads_chiller_modbus_dev_t *) malloc(ads_chiller_modbus_devs_count * sizeof(ads_chiller_modbus_dev_t));
        if (pool == NULL) {
            debug("Memory allocation failed for pool");
            return EAR_ERROR;
        }
        ads_chiller_pool_registers = calloc(ads_chiller_modbus_devs_count, sizeof(ads_chiller_t));
        register_to_enum =
            (ads_chiller_metric_type **) malloc(ads_chiller_modbus_devs_count * sizeof(ads_chiller_metric_type *));
        enum_to_register = (int **) malloc(ads_chiller_modbus_devs_count * sizeof(int *));
        if (!ads_chiller_pool_registers || !register_to_enum || !enum_to_register) {
            debug("Memory allocation failed");
            return EAR_ERROR;
        }

        for (int i = 0; i < ads_chiller_modbus_devs_count; i++) {
            register_to_enum[i] =
                (ads_chiller_metric_type *) malloc(ADS_CHILLER_MAX_METRIC * sizeof(ads_chiller_metric_type));
            enum_to_register[i] = (int *) malloc(ADS_CHILLER_MAX_METRIC * sizeof(int));
            for (int j = 0; j < ADS_CHILLER_MAX_METRIC; j++) {
                register_to_enum[i][j] = ADS_CHILLER_UNKNOWN_METRIC;
                enum_to_register[i][j] = -1;
            }
        }

        for (uint i = 0; i < conf->cluster.num_edcmons_tags; i++) {
            if (conf->cluster.edcmon_tags[i].type == ads_chiller) {
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
                    for (int r = 0; r < ADS_CHILLER_MAX_METRIC; r++) {
                        registers[r].name = strdup(ads_chiller_metric_str(r));
                        registers[r].reg  = r;
                    }
                    num_registers   = ADS_CHILLER_MAX_METRIC;
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
                    ads_chiller_modbus_devs_count--;
                    free(register_to_enum[ads_chiller_modbus_devs_count]);
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
        s = EAR_SUCCESS;
    }

    ads_chiller_initialized = 1;
    return s;
}

state_t ads_chiller_modbus_dispose(ctx_t *c)
{
    if (ads_chiller_initialized) {
        if (ads_chiller_sus != NULL) {
            monitor_unregister(ads_chiller_sus);
        }
        pthread_mutex_lock(&lock);
        ads_chiller_initialized = 0;
        for (int i = 0; i < ads_chiller_modbus_devs_count; i++) {
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
        free(ads_chiller_pool_registers);
        ads_chiller_modbus_devs_count = 0;

        pthread_mutex_unlock(&lock);
    }
    return EAR_SUCCESS;
}

static int is_working(int i)
{
    // TODO
    return 1;
}

state_t ads_chiller_modbus_static_read(int i, ads_chiller_t *metric)
{
    int s;

    memset(metric_register_data, 0, sizeof(uint16_t) * (ADS_CHILLER_MAX_METRIC + 1));

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

    return s;
}

void ads_chiller_modbus_get_value(ctx_t *c, ads_chiller_metric_type metric, int dev_idx, uint16_t *value)
{
    if (metric >= ADS_CHILLER_MAX_METRIC) {
        *value = -1;
        return;
    }
    emodbus_read(&pool[dev_idx].dev, enum_to_register[dev_idx][metric], value);

    ads_chiller_pool_registers[dev_idx].register_data[enum_to_register[dev_idx][metric]] = *value;
}

void ads_chiller_modbus_get_value_all(ctx_t *c, ads_chiller_metric_type metric, uint16_t *values)
{
    if (metric >= ADS_CHILLER_MAX_METRIC) {
        for (int i = 0; i < ads_chiller_modbus_devs_count; i++) {
            values[i] = -1;
        }
        return;
    }
    for (int i = 0; i < ads_chiller_modbus_devs_count; i++) {
        emodbus_read(&pool[i].dev, enum_to_register[i][metric], &values[i]);
        ads_chiller_pool_registers[i].register_data[enum_to_register[i][metric]] = values[i];
    }
}

state_t ads_chiller_modbus_set_value(ctx_t *c, ads_chiller_metric_type metric, int dev_idx, double value)
{
    uint16_t reg_value;

    if (metric >= ADS_CHILLER_MAX_METRIC || dev_idx < 0 || dev_idx >= ads_chiller_modbus_devs_count) {
        return EAR_ERROR;
    }

    reg_value = (uint16_t) value;

    if (state_fail(emodbus_write(&pool[dev_idx].dev, enum_to_register[dev_idx][metric], reg_value))) {
        return EAR_ERROR;
    }

    ads_chiller_pool_registers[dev_idx].register_data[enum_to_register[dev_idx][metric]] = (uint32_t) reg_value;
    return EAR_SUCCESS;
}

state_t ads_chiller_modbus_set_value_all(ctx_t *c, ads_chiller_metric_type metric, const double *values)
{
    int i;

    if (metric >= ADS_CHILLER_MAX_METRIC) {
        return EAR_ERROR;
    }

    for (i = 0; i < ads_chiller_modbus_devs_count; i++) {
        if (state_fail(ads_chiller_modbus_set_value(c, metric, i, values[i]))) {
            error("Failed to write value to device %d", i);
        }
    }
    return EAR_SUCCESS;
}

void ads_chiller_modbus_reorder_metrics(ads_chiller_t *data, int i)
{
    for (int r = 0; r < ads_chiller_pool_registers[i].num_registers; ++r) {
        // ads_chiller_pool_registers[i].register_data[register_to_enum[i][r]] += metric.register_data[r];
        data[i].register_data[register_to_enum[i][r]] = ads_chiller_pool_registers[i].register_data[r];
        debug("Device %d, register %d: %s = %d", i, r, ads_chiller_metric_str(register_to_enum[i][r]),
              data[i].register_data[register_to_enum[i][r]]);
    }
}

state_t ads_chiller_modbus_read_raw(ctx_t *c, ads_chiller_t *data)
{
    timestamp_t time;
    int i;
    timestamp_getfast(&time);
    for (i = 0; i < ads_chiller_modbus_devs_count; ++i) {
        ads_chiller_modbus_static_read(
            i, &data[i]); // WARNING: Data is not correctly sorted here! enum will not match metrics
        data[i].time = time;
    }
    return EAR_SUCCESS;
}

// state_t ads_chiller_modbus_write(ctx_t *c, ads_chiller_metric_type metric, uint *values)
// {
//     // TODO
//     return EAR_SUCCESS;
// }

float ads_chiller_modbus_get_metric_mult()
{
    return (float) METRIC_MULT;
}

int ads_chiller_modbus_get_is_kelvin()
{
    return TEMP_KELVIN;
}
