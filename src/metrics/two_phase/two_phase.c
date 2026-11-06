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
#include <stdlib.h>
#include <string.h>
// #include <common/types/configuration/cluster_conf.h>
#include <common/states.h>
#include <common/string_enhanced.h>
#include <common/types.h>

#include <metrics/two_phase/archs/dummy.h>
#include <metrics/two_phase/archs/modbus.h>
#include <metrics/two_phase/two_phase.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static two_phase_ops_t ops;
static two_phase_devs_t *devs = NULL;
static uint devs_count;
static uint ok_load;
static float mult = 1.0;
static int is_k   = 0;

two_phase_metric_type two_phase_get_metric_enum(const char *string)
{
    two_phase_metric_type result = TWO_PHASE_UNKNOWN_METRIC;
    char *name                   = strdup(string);
    strtoup(name);

    if (strcmp(name, "IN_TEMP_HT") == 0)
        result = TP_IN_TEMP_HT;
    else if (strcmp(name, "OUT_TEMP_HT") == 0)
        result = TP_OUT_TEMP_HT;
    else if (strcmp(name, "FLOWRATE_HT") == 0)
        result = TP_FLOWRATE_HT;
    else if (strcmp(name, "FLOWRATE_HT_CTRL") == 0)
        result = TP_FLOWRATE_HT_CTRL;

    free(name);
    return result;
}

const char *two_phase_metric_str(two_phase_metric_type metric)
{
    switch (metric) {
        case TP_IN_TEMP_HT:
            return "IN_TEMP_HT";
        case TP_OUT_TEMP_HT:
            return "OUT_TEMP_HT";
        case TP_FLOWRATE_HT:
            return "FLOWRATE_HT";
        case TP_FLOWRATE_HT_CTRL:
            return "FLOWRATE_HT_CTRL";
        default:
            return "TWO_PHASE_UNKNOWN_METRIC";
    }
}

// Initializes the context. The caller already holds `lock`, so this must not take it again:
// the mutex is not recursive and the trylock loops in this file would spin forever.
static state_t static_init(ctx_t *c)
{
    state_t s = EAR_SUCCESS;

    if (ops.init != NULL) {
        if (state_fail(s = ops.init(c))) {
            return s;
        }
    }
    // Number of devices are used in data functions
    two_phase_get_devices(&devs, &devs_count);
    return s;
}

state_t two_phase_load(ctx_t *c, int force_api)
{
    state_t s = EAR_SUCCESS;

    while (pthread_mutex_trylock(&lock))
        ;
    if (ok_load) {
        goto unlock_load;
    }
    if (API_IS(force_api, API_DUMMY)) {
        goto dummy;
    }

    two_phase_modbus_load(&ops, force_api);
dummy:
    two_phase_dummy_load(&ops);
    mult = ops.get_metric_mult();
    is_k = ops.get_is_kelvin();
    // Discovery and context initialization are one step: there is no useful state between them,
    // and splitting them let callers read devices before the arch had counted any.
    if (state_fail(s = static_init(c))) {
        goto unlock_load;
    }
    ok_load = 1;
unlock_load:
    pthread_mutex_unlock(&lock);
    return s;
}

void two_phase_get_api(uint *api)
{
    apinfo_t info;
    two_phase_get_info(&info);
    *api = info.api;
}

void two_phase_get_info(apinfo_t *info)
{
    info->layer       = "TWO_PHASE";
    info->api         = API_NONE;
    info->devs_count  = 0;
    info->scope       = SCOPE_RACK; // This could also be SCOPE_NODE or SCOPE_CLUSTER
    info->granularity = GRANULARITY_PERIPHERAL;
    if (ops.get_info != NULL) {
        ops.get_info(info);
    }
}

void two_phase_get_devices(two_phase_devs_t **devs, uint *devs_count)
{
    if (ops.get_devices != NULL) {
        ops.get_devices(devs, devs_count);
    }
}

float two_phase_get_metric_mult()
{
    return mult;
}

int two_phase_get_is_kelvin()
{
    return is_k;
}

state_t two_phase_dispose(ctx_t *c)
{
    state_t s = EAR_SUCCESS;
    while (pthread_mutex_trylock(&lock))
        ;
    if (ops.dispose != NULL) {
        s = ops.dispose(c);
    }
    if (devs != NULL) {
        free(devs);
        devs = NULL;
    }
    // load() now carries the initialization, so it has to be re-runnable after a dispose.
    ok_load = 0;
    pthread_mutex_unlock(&lock);
    return s;
}

state_t two_phase_read(ctx_t *c, two_phase_t *data)
{
    preturn(ops.read, c, data);
}

state_t two_phase_read_raw(ctx_t *c, two_phase_t *data)
{
    preturn(ops.read_raw, c, data);
}

state_t two_phase_read_diff(ctx_t *c, two_phase_t *data2, two_phase_t *data1, two_phase_t *data_diff)
{
    state_t s;
    if (state_fail(s = two_phase_read(c, data2))) {
        return s;
    }
    two_phase_data_diff(data2, data1, data_diff);
    return s;
}

state_t two_phase_read_copy(ctx_t *c, two_phase_t *data2, two_phase_t *data1, two_phase_t *data_diff)
{
    state_t s;
    if (state_fail(s = two_phase_read_diff(c, data2, data1, data_diff))) {
        return s;
    }
    two_phase_data_copy(data1, data2);
    return s;
}

static void static_data_diff(two_phase_t *data2, two_phase_t *data1, two_phase_t *data_diff, int i)
{
    two_phase_t *d3 = &data_diff[i];
    two_phase_t *d2 = &data2[i];
    two_phase_t *d1 = &data1[i];

    // if (d2->correct != 1 || d1->correct != 1) {
    // 	return;
    // }

    if ((d3->samples = d2->samples - d1->samples) == 0) {
        memset(d3, 0, sizeof(two_phase_t));
        return;
    }
    // for (int r = 0; r < d1->num_registers; ++r) {  // I can't use num_registers because
    //  registers order can be different here
    for (int r = 0; r < TWO_PHASE_MAX_METRIC; ++r) {

        d3->register_data[r] = (d2->register_data[r] - d1->register_data[r]) / d3->samples;
    }
}

void two_phase_data_diff(two_phase_t *data2, two_phase_t *data1, two_phase_t *data_diff)
{
    int i;
    two_phase_data_null(data_diff);
    for (i = 0; i < devs_count; i++) {
        static_data_diff(data2, data1, data_diff, i);
    }
}

void two_phase_data_merge(two_phase_t *data_diff, two_phase_t *data_merge)
{
    int i;

    memset((void *) data_merge, 0, sizeof(two_phase_t));

    for (i = 0; i < devs_count; ++i) {
        for (int r = 0; r < TWO_PHASE_MAX_METRIC; ++r) {
            data_merge->register_data[r] += data_diff[i].register_data[r];
        }
    }
    // Static
    data_merge->time    = data_diff[0].time;
    data_merge->samples = data_diff[0].samples;
    // Averages
    for (int r = 0; r < TWO_PHASE_MAX_METRIC; ++r) {
        data_merge->register_data[r] /= devs_count;
    }
}

void two_phase_data_alloc(two_phase_t **data)
{
    *data = calloc(devs_count, sizeof(two_phase_t));
}

void two_phase_data_free(two_phase_t **data)
{
    free(*data);
    *data = NULL;
}

void two_phase_data_null(two_phase_t *data)
{
    if (data != NULL) {
        memset(data, 0, devs_count * sizeof(two_phase_t));
    }
}

void two_phase_data_copy(two_phase_t *data_dst, two_phase_t *data_src)
{
    memcpy(data_dst, data_src, devs_count * sizeof(two_phase_t));
}

void two_phase_data_print(two_phase_t *data, int fd)
{
    int i;
    for (i = 0; i < devs_count; ++i) {
        dprintf(fd, "two phase %u: %4lu\n", i, data[i].samples);
    }
}

char *two_phase_data_tostr(two_phase_t *data, char *buffer, int length)
{
    int accuml;
    size_t s;
    int i;

    for (i = accuml = 0; i < devs_count && length > 0; ++i) {
        s      = snprintf(&buffer[accuml], length, "two phase %u: , %4lu\n", i, data[i].samples);
        length = length - s;
        accuml = accuml + s;
    }
    return buffer;
}

double two_phase_get_metric(two_phase_t *data, two_phase_metric_type metric)
{
    if (metric >= TWO_PHASE_MAX_METRIC || data == NULL) {
        return -1.0;
    }
    double value = (double) data->register_data[metric];

    if (metric == TP_OUT_TEMP_HT || metric == TP_IN_TEMP_HT) {
        if (mult > 0)
            value /= (double) mult;
        if (is_k)
            value -= 273.15;
    } else {
        if (mult > 0)
            value /= (double) mult;
    }

    return value;
}

int two_phase_is_supported()
{
    // return two_phase_eard_is_supported();
    return 1;
}
