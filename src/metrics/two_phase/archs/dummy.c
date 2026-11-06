/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#include <common/states.h>
#include <common/types.h>
#include <metrics/two_phase/archs/dummy.h>
#include <metrics/two_phase/two_phase.h>

#define FAKE_TWO_PHASE 1

static two_phase_devs_t dummy_devs[1];
static uint dummy_devs_count = 1;

void two_phase_dummy_load(two_phase_ops_t *ops)
{
    apis_put(ops->get_info, two_phase_dummy_get_info);
    apis_put(ops->get_devices, two_phase_dummy_get_devices);
    apis_put(ops->init, two_phase_dummy_init);
    apis_put(ops->dispose, two_phase_dummy_dispose);
    apis_put(ops->read, two_phase_dummy_read);
    apis_put(ops->read_raw, two_phase_dummy_read_raw);
    apis_put(ops->get_metric_mult, two_phase_dummy_get_metric_mult);
    apis_put(ops->get_is_kelvin, two_phase_dummy_get_is_kelvin);
}

void two_phase_dummy_get_info(apinfo_t *info)
{
    info->api        = API_DUMMY;
    info->devs_count = dummy_devs_count;
}

void two_phase_dummy_get_devices(two_phase_devs_t **devs, uint *devs_count)
{
    if (devs != NULL) {
        // Hand back an owned copy, not the static array: the caller (two_phase_dispose) frees
        // what it got, and the modbus arch allocates here too.
        *devs = calloc(dummy_devs_count, sizeof(two_phase_devs_t));
        if (*devs != NULL) {
            memcpy(*devs, dummy_devs, dummy_devs_count * sizeof(two_phase_devs_t));
        }
    }
    if (devs_count != NULL) {
        *devs_count = dummy_devs_count;
    }
}

state_t two_phase_dummy_init(ctx_t *c)
{
    // Initialize dummy device
    dummy_devs[0].serial = 0;
    dummy_devs[0].index  = 0;

    return EAR_SUCCESS;
}

float two_phase_dummy_get_metric_mult()
{
    return 1.0;
}

int two_phase_dummy_get_is_kelvin()
{
    return 0;
}

state_t two_phase_dummy_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t two_phase_dummy_read(ctx_t *c, two_phase_t *data)
{
    // Fill with dummy data
#if FAKE_TWO_PHASE
    for (int i = 0; i < dummy_devs_count; i++) {
        data[i].time.tv_nsec  = 0;
        data[i].time.tv_sec   = 0;
        data[i].num_registers = TWO_PHASE_MAX_METRIC; // Number of metrics in two_phase_metric_type

        // Set some dummy values for registers
        for (int j = 0; j < data[i].num_registers; j++) {
            data[i].register_data[j] = 200 + j;
        }
    }
#else
    two_phase_data_null(data);
#endif
    return EAR_SUCCESS;
}

state_t two_phase_dummy_read_raw(ctx_t *c, two_phase_t *data)
{
    // Same as regular read for dummy implementation
    return two_phase_dummy_read(c, data);
}