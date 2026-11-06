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
#include <metrics/ads_chiller/ads_chiller.h>
#include <metrics/ads_chiller/archs/dummy.h>

#define FAKE_ADS_CHILLER 1

static ads_chiller_devs_t dummy_devs[1];
static uint dummy_devs_count = 1;

void ads_chiller_dummy_load(ads_chiller_ops_t *ops)
{
    apis_put(ops->get_info, ads_chiller_dummy_get_info);
    apis_put(ops->get_devices, ads_chiller_dummy_get_devices);
    apis_put(ops->init, ads_chiller_dummy_init);
    apis_put(ops->dispose, ads_chiller_dummy_dispose);
    apis_put(ops->read, ads_chiller_dummy_read);
    apis_put(ops->read_raw, ads_chiller_dummy_read_raw);
    apis_put(ops->get_metric_mult, ads_chiller_dummy_get_metric_mult);
    apis_put(ops->get_is_kelvin, ads_chiller_dummy_get_is_kelvin);
}

void ads_chiller_dummy_get_info(apinfo_t *info)
{
    info->api        = API_DUMMY;
    info->devs_count = dummy_devs_count;
}

void ads_chiller_dummy_get_devices(ads_chiller_devs_t **devs, uint *devs_count)
{
    if (devs != NULL) {
        // Hand back an owned copy, not the static array: the caller (ads_chiller_dispose) frees
        // what it got, and the modbus arch allocates here too.
        *devs = calloc(dummy_devs_count, sizeof(ads_chiller_devs_t));
        if (*devs != NULL) {
            memcpy(*devs, dummy_devs, dummy_devs_count * sizeof(ads_chiller_devs_t));
        }
    }
    if (devs_count != NULL) {
        *devs_count = dummy_devs_count;
    }
}

state_t ads_chiller_dummy_init(ctx_t *c)
{
    // Initialize dummy device
    dummy_devs[0].serial = 0;
    dummy_devs[0].index  = 0;

    return EAR_SUCCESS;
}

float ads_chiller_dummy_get_metric_mult()
{
    return 1.0;
}

int ads_chiller_dummy_get_is_kelvin()
{
    return 0;
}

state_t ads_chiller_dummy_dispose(ctx_t *c)
{
    return EAR_SUCCESS;
}

state_t ads_chiller_dummy_read(ctx_t *c, ads_chiller_t *data)
{
    // Fill with dummy data
#if FAKE_ADS_CHILLER
    for (int i = 0; i < dummy_devs_count; i++) {
        data[i].time.tv_nsec  = 0;
        data[i].time.tv_sec   = 0;
        data[i].num_registers = ADS_CHILLER_MAX_METRIC; // Number of metrics in ads_chiller_metric_type

        // Set some dummy values for registers
        for (int j = 0; j < data[i].num_registers; j++) {
            data[i].register_data[j] = 200 + j;
        }
    }
#else
    ads_chiller_data_null(data);
#endif
    return EAR_SUCCESS;
}

state_t ads_chiller_dummy_read_raw(ctx_t *c, ads_chiller_t *data)
{
    // Same as regular read for dummy implementation
    return ads_chiller_dummy_read(c, data);
}