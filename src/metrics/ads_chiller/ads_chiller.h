/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef METRICS_ADS_CHILLER_H
#define METRICS_ADS_CHILLER_H

#include <common/includes.h>
#include <common/plugins.h>
#include <common/states.h>
#include <common/system/time.h>
#include <common/types.h>
#include <metrics/common/apis.h>

// Default metrics order, use the ear.conf
// if the real order is different or to select only a subset
// of metrics to read.

typedef enum {
    AC_IN_TEMP_HT,
    AC_OUT_TEMP_HT,
    AC_IN_TEMP_MT,
    AC_OUT_TEMP_MT,
    AC_IN_TEMP_LT,
    AC_OUT_TEMP_LT,
    AC_VAC_PRESS1,
    AC_VAC_PRESS2,
    AC_FLOWRATE_HT,
    AC_FLOWRATE_HT_CTRL,
    AC_FLOWRATE_MT,
    AC_FLOWRATE_MT_CTRL,
    AC_FLOWRATE_LT,
    AC_FLOWRATE_LT_CTRL,
    AC_EVAPORATOR_TEMP,
    AC_CONDENSER_TEMP,
    AC_PHASE_TIME_1,
    AC_PHASE_TIME_2,
    AC_PHASE_TIME_3,
    AC_PHASE_TIME_4,
    AC_PHASE_TIME_5,
    AC_PHASE,
    AC_WORKING_MODE,
    // The cooling plant's own electrical draw (W), as reported by the chiller.
    // The DEFAULT register mapping is positional (register index == enum value, see
    // ads_chiller_modbus_load_registers()); it only applies when an edcmontag declares no
    // sensor_list. Real deployments name their registers and are unaffected.
    AC_CHILLER_POWER,
    AC_PUMP_POWER,
    ADS_CHILLER_UNKNOWN_METRIC, // Fallback for unknown metrics
    ADS_CHILLER_MAX_METRIC = ADS_CHILLER_UNKNOWN_METRIC
} ads_chiller_metric_type;

#define ADS_CHILLER_ACTUATORS    10
#define ADS_CHILLER_READ_METRICS (ADS_CHILLER_MAX_METRIC)

static const ads_chiller_metric_type ads_chiller_actuators[ADS_CHILLER_ACTUATORS] = {
    AC_FLOWRATE_HT_CTRL, AC_FLOWRATE_MT_CTRL, AC_FLOWRATE_LT_CTRL, AC_PHASE_TIME_1, AC_PHASE_TIME_2,
    AC_PHASE_TIME_3,     AC_PHASE_TIME_4,     AC_PHASE_TIME_5,     AC_PHASE,        AC_WORKING_MODE};

typedef struct ads_chiller_s {
    timestamp_t time;
    ulong samples;
    uint32_t register_data[ADS_CHILLER_MAX_METRIC + 1]; // use in combination with ads_chiller_metric_type enum
    int num_registers;
    uint working;
} ads_chiller_t;

typedef struct ads_chiller_devs_s {
    ullong serial;
    uint index;
} ads_chiller_devs_t;

typedef struct ads_chiller_ops_s {
    void (*get_info)(apinfo_t *info);
    void (*get_devices)(ads_chiller_devs_t **devs, uint *devs_count);
    state_t (*init)(ctx_t *c);
    state_t (*dispose)(ctx_t *c);
    state_t (*read)(ctx_t *c, ads_chiller_t *data);
    state_t (*read_raw)(ctx_t *c, ads_chiller_t *data);
    float (*get_metric_mult)();
    int (*get_is_kelvin)();
} ads_chiller_ops_t;

float ads_chiller_get_metric_mult();
int ads_chiller_get_is_kelvin();
ads_chiller_metric_type ads_chiller_get_metric_enum(const char *string);
const char *ads_chiller_metric_str(ads_chiller_metric_type metric);

void ads_chiller_get_api(uint *api);

// Discovers the low level API and initializes the context in one step.
state_t ads_chiller_load(ctx_t *c, int force_api);

// Returns all available static information (Work in progress)
void ads_chiller_get_info(apinfo_t *info);

// Count devices by calling get_devices
#define ads_chiller_count_devices(c, devs_count) ads_chiller_get_devices(NULL, devs_count)

// Information about devices
void ads_chiller_get_devices(ads_chiller_devs_t **devs, uint *devs_count);

state_t ads_chiller_dispose(ctx_t *c);

// Reads the chiller device data and stores it in ads_chiller_t (1 per device).
state_t ads_chiller_read(ctx_t *c, ads_chiller_t *data);

// Reads the device data directly from the hardware (not pooled).
state_t ads_chiller_read_raw(ctx_t *c, ads_chiller_t *data);

// Performs a ads_chiller_read() over data2, a ads_chiller_data_diff() and copies data2 in data1.
state_t ads_chiller_read_diff(ctx_t *c, ads_chiller_t *data2, ads_chiller_t *data1, ads_chiller_t *data_diff);

// Performs a ads_chiller_data_diff() and then it copies data2 in data1.
state_t ads_chiller_read_copy(ctx_t *c, ads_chiller_t *data2, ads_chiller_t *data1, ads_chiller_t *data_diff);

/** Helpers */
// Substracts the elements of the ads_chiller_t array (data_diff = data2 - data1).
void ads_chiller_data_diff(ads_chiller_t *data2, ads_chiller_t *data1, ads_chiller_t *data_diff);

// Makes an average of all the elements of the data_diff array.
void ads_chiller_data_merge(ads_chiller_t *data_diff, ads_chiller_t *data_merge);

// Allocates an array of ads_chiller_t (1 per device).
void ads_chiller_data_alloc(ads_chiller_t **data);

// Frees an array of ads_chiller_t.
void ads_chiller_data_free(ads_chiller_t **data);

// Sets to 0 an array of ads_chiller_t.
void ads_chiller_data_null(ads_chiller_t *data);

// Copies an array of ads_chiller_t.
void ads_chiller_data_copy(ads_chiller_t *data_dst, ads_chiller_t *data_src);

// Prints an array of ads_chiller_t in the channel fd.
void ads_chiller_data_print(ads_chiller_t *data, int fd);

// Copies the printing string in a char buffer.
char *ads_chiller_data_tostr(ads_chiller_t *data, char *buffer, int length);

int ads_chiller_is_supported();

double ads_chiller_get_metric(ads_chiller_t *data, ads_chiller_metric_type metric);

#endif // METRICS_ADS_CHILLER_H
