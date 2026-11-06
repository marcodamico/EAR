/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#define SHOW_DEBUGS 1
#include <common/output/debug.h>
#include <common/system/monitor.h>
#include <data_center_monitor/plugins/conf.h>
#include <metrics/ads_chiller/ads_chiller.h>
#include <metrics/ads_chiller/archs/modbus.h>
#include <metrics/common/modbus/ads_chiller_modbus.h>
#include <metrics/common/modbus/emodbus.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void ads_chiller_modbus_load(ads_chiller_ops_t *ops, int force_api)
{
    base_ads_chiller_modbus_load(ops, force_api);
    apis_set(ops->init, ads_chiller_modbus_init);
    apis_set(ops->read, ads_chiller_modbus_read);

    debug("Loaded MODBUS");
}

state_t ads_chiller_modbus_init(ctx_t *c)
{
    state_t s = base_ads_chiller_modbus_init(c);
    if (state_fail(s)) {
        return s;
    }
    if (ads_chiller_ok_pool) {
        // Initializing monitoring thread suscription.
        ads_chiller_sus            = suscription();
        ads_chiller_sus->call_main = ads_chiller_modbus_pool;
        // Poll periods in milliseconds.
        ads_chiller_sus->time_relax = 2000;
        ads_chiller_sus->time_burst = 1000;
        // Initializing monitoring thread.
        if (state_ok(s = ads_chiller_sus->suscribe(ads_chiller_sus))) {
            ads_chiller_initialized = 1;
        }
    } // ads_chiller_ok_pool
    return s;
}

state_t ads_chiller_modbus_pool(void *p)
{
    timestamp_t time;
    // double time_diff;
    ads_chiller_t metric;

    int i;

    debug("Pooling");
    uint working = 0;
    pthread_mutex_lock(&lock);

    timestamp_getfast(&time);

    for (i = 0; i < ads_chiller_modbus_devs_count; ++i) {
        if (state_fail(ads_chiller_modbus_static_read(i, &metric))) {
            debug("Error reading device %d: %s", i, state_msg);
            continue;
        }
        // Pooling the data
        ads_chiller_pool_registers[i].time = time;
        ads_chiller_pool_registers[i].samples += metric.samples;
        ads_chiller_pool_registers[i].num_registers = metric.num_registers;
        // Copying the data
        for (int r = 0; r < metric.num_registers; ++r) {
            ads_chiller_pool_registers[i].register_data[r] += metric.register_data[r];
        }

        ads_chiller_pool_registers[i].working = metric.working;
        // Burst or not
        working += ads_chiller_pool_registers[i].working;
    }
    // Lock
    pthread_mutex_unlock(&lock);

    return EAR_SUCCESS;
}

/* Reads all data from all the devices */
state_t ads_chiller_modbus_read(ctx_t *c, ads_chiller_t *data)
{
    // Updating pool
    ads_chiller_modbus_pool(NULL);
    pthread_mutex_lock(&lock);

    // Reordering metrics according to the enum
    for (int i = 0; i < ads_chiller_modbus_devs_count; ++i) {
        ads_chiller_modbus_reorder_metrics(data, i);

        data[i].time          = ads_chiller_pool_registers[i].time;
        data[i].samples       = ads_chiller_pool_registers[i].samples;
        data[i].num_registers = ads_chiller_pool_registers[i].num_registers;
        data[i].working       = ads_chiller_pool_registers[i].working;
    }

    // memcpy(data, ads_chiller_pool_registers, devs_count * sizeof(ads_chiller_t));
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}