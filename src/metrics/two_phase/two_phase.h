/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef METRICS_TWO_PHASE_H
#define METRICS_TWO_PHASE_H

#include <common/includes.h>
#include <common/plugins.h>
#include <common/states.h>
#include <common/system/time.h>
#include <common/types.h>
#include <metrics/common/apis.h>

// Internal code metrics order, must be kept in sync with
// the metrics in the config file.

typedef enum {
    TP_IN_TEMP_HT,
    TP_OUT_TEMP_HT,
    TP_FLOWRATE_HT,
    TP_FLOWRATE_HT_CTRL,
    TWO_PHASE_UNKNOWN_METRIC, // Fallback for unknown metrics
    TWO_PHASE_MAX_METRIC = TWO_PHASE_UNKNOWN_METRIC
} two_phase_metric_type;

#define TWO_PHASE_ACTUATORS    1
#define TWO_PHASE_READ_METRICS (TWO_PHASE_MAX_METRIC)

static const two_phase_metric_type two_phase_actuators[TWO_PHASE_ACTUATORS] = {TP_FLOWRATE_HT_CTRL};

typedef struct two_phase_s {
    timestamp_t time;
    ulong samples;
    uint32_t register_data[TWO_PHASE_MAX_METRIC + 1]; // data might not be in the enum order
    int num_registers;
    uint working;
} two_phase_t;

typedef struct two_phase_devs_s {
    ullong serial;
    uint index;
} two_phase_devs_t;

typedef struct two_phase_ops_s {
    void (*get_info)(apinfo_t *info);
    void (*get_devices)(two_phase_devs_t **devs, uint *devs_count);
    state_t (*init)(ctx_t *c);
    state_t (*dispose)(ctx_t *c);
    state_t (*read)(ctx_t *c, two_phase_t *data);
    state_t (*read_raw)(ctx_t *c, two_phase_t *data);
    float (*get_metric_mult)();
    int (*get_is_kelvin)();
} two_phase_ops_t;

float two_phase_get_metric_mult();
int two_phase_get_is_kelvin();
two_phase_metric_type two_phase_get_metric_enum(const char *string);
const char *two_phase_metric_str(two_phase_metric_type metric);

void two_phase_get_api(uint *api);

// Discovers the low level API and initializes the context in one step.
state_t two_phase_load(ctx_t *c, int force_api);

// Returns all available static information (Work in progress)
void two_phase_get_info(apinfo_t *info);

// Count devices by calling get_devices
#define two_phase_count_devices(c, devs_count) two_phase_get_devices(NULL, devs_count)

// Information about devices
void two_phase_get_devices(two_phase_devs_t **devs, uint *devs_count);

state_t two_phase_dispose(ctx_t *c);

// Reads the chiller device data and stores it in two_phase_t (1 per device).
state_t two_phase_read(ctx_t *c, two_phase_t *data);

// Reads the device data directly from the hardware (not pooled).
state_t two_phase_read_raw(ctx_t *c, two_phase_t *data);

// Performs a two_phase_read() over data2, a two_phase_data_diff() and copies data2 in data1.
state_t two_phase_read_diff(ctx_t *c, two_phase_t *data2, two_phase_t *data1, two_phase_t *data_diff);

// Performs a two_phase_data_diff() and then it copies data2 in data1.
state_t two_phase_read_copy(ctx_t *c, two_phase_t *data2, two_phase_t *data1, two_phase_t *data_diff);

/** Helpers */
// Substracts the elements of the two_phase_t array (data_diff = data2 - data1).
void two_phase_data_diff(two_phase_t *data2, two_phase_t *data1, two_phase_t *data_diff);

// Makes an average of all the elements of the data_diff array.
void two_phase_data_merge(two_phase_t *data_diff, two_phase_t *data_merge);

// Allocates an array of two_phase_t (1 per device).
void two_phase_data_alloc(two_phase_t **data);

// Frees an array of two_phase_t.
void two_phase_data_free(two_phase_t **data);

// Sets to 0 an array of two_phase_t.
void two_phase_data_null(two_phase_t *data);

// Copies an array of two_phase_t.
void two_phase_data_copy(two_phase_t *data_dst, two_phase_t *data_src);

// Prints an array of two_phase_t in the channel fd.
void two_phase_data_print(two_phase_t *data, int fd);

// Copies the printing string in a char buffer.
char *two_phase_data_tostr(two_phase_t *data, char *buffer, int length);

int two_phase_is_supported();

double two_phase_get_metric(two_phase_t *data, two_phase_metric_type metric);

#endif // METRICS_TWO_PHASE_H
