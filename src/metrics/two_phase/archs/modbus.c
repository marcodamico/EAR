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
#include <metrics/common/modbus/emodbus.h>
#include <metrics/common/modbus/two_phase_modbus.h>
#include <metrics/two_phase/archs/modbus.h>
#include <metrics/two_phase/two_phase.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void two_phase_modbus_load(two_phase_ops_t *ops, int force_api)
{
    base_two_phase_modbus_load(ops, force_api);
    apis_set(ops->read, two_phase_modbus_read);

    debug("Loaded MODBUS");
}

state_t two_phase_modbus_init(ctx_t *c)
{
    state_t s = base_two_phase_modbus_init(c);
    if (state_fail(s)) {
        return s;
    }
    if (two_phase_ok_pool) {
        // Initializing monitoring thread suscription.
        two_phase_sus             = suscription();
        two_phase_sus->call_main  = two_phase_modbus_pool;
        two_phase_sus->time_relax = 2000;
        two_phase_sus->time_burst = 1000;
        // Initializing monitoring thread.
        if (state_fail(s = two_phase_sus->suscribe(two_phase_sus))) {
            two_phase_initialized = 0;
        }
    } // ok_pool
    return s;
}

state_t two_phase_modbus_pool(void *p)
{
    timestamp_t time;
    // double time_diff;
    two_phase_t metric;

    int i;

    debug("Pooling");
    uint working = 0;
    pthread_mutex_lock(&lock);

    timestamp_getfast(&time);

    for (i = 0; i < two_phase_modbus_devs_count; ++i) {
        if (!two_phase_modbus_static_read(i, &metric)) {
            debug("Error reading device %d: %s", i, state_msg);
            continue;
        }
        // Pooling the data
        two_phase_pool_registers[i].time = time;
        two_phase_pool_registers[i].samples += metric.samples;
        two_phase_pool_registers[i].num_registers = metric.num_registers;
        // Copying the data
        for (int r = 0; r < metric.num_registers; ++r) {
            two_phase_pool_registers[i].register_data[r] += metric.register_data[r];
        }

        two_phase_pool_registers[i].working = metric.working;
        // Burst or not
        working += two_phase_pool_registers[i].working;
    }
    // Lock
    pthread_mutex_unlock(&lock);

    return EAR_SUCCESS;
}

/* Reads all data from all the devices */
state_t two_phase_modbus_read(ctx_t *c, two_phase_t *data)
{
    // Updating pool
    two_phase_modbus_pool(NULL);
    pthread_mutex_lock(&lock);

    // Reordering metrics according to the enum
    for (int i = 0; i < two_phase_modbus_devs_count; ++i) {
        two_phase_reorder_metrics(data, i);

        data[i].time          = two_phase_pool_registers[i].time;
        data[i].samples       = two_phase_pool_registers[i].samples;
        data[i].num_registers = two_phase_pool_registers[i].num_registers;
        data[i].working       = two_phase_pool_registers[i].working;
    }

    // memcpy(data, two_phase_pool_registers, devs_count * sizeof(two_phase_t));
    pthread_mutex_unlock(&lock);
    return EAR_SUCCESS;
}