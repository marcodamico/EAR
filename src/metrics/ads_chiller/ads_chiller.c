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

#include <metrics/ads_chiller/ads_chiller.h>
#include <metrics/ads_chiller/archs/dummy.h>
#include <metrics/ads_chiller/archs/modbus.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static ads_chiller_ops_t ops;
static ads_chiller_devs_t *devs = NULL;
static uint devs_count;
static uint ok_load;

static float mult = 1.0;
static int is_k   = 1;

ads_chiller_metric_type ads_chiller_get_metric_enum(const char *string)
{
    ads_chiller_metric_type result = ADS_CHILLER_UNKNOWN_METRIC;
    char *name                     = strdup(string);
    strtoup(name);

    if (strcmp(name, "IN_TEMP_HT") == 0)
        result = AC_IN_TEMP_HT;
    else if (strcmp(name, "OUT_TEMP_HT") == 0)
        result = AC_OUT_TEMP_HT;
    else if (strcmp(name, "IN_TEMP_MT") == 0)
        result = AC_IN_TEMP_MT;
    else if (strcmp(name, "OUT_TEMP_MT") == 0)
        result = AC_OUT_TEMP_MT;
    else if (strcmp(name, "IN_TEMP_LT") == 0)
        result = AC_IN_TEMP_LT;
    else if (strcmp(name, "OUT_TEMP_LT") == 0)
        result = AC_OUT_TEMP_LT;
    else if (strcmp(name, "VAC_PRESS1") == 0)
        result = AC_VAC_PRESS1;
    else if (strcmp(name, "VAC_PRESS2") == 0)
        result = AC_VAC_PRESS2;
    else if (strcmp(name, "FLOWRATE_HT") == 0)
        result = AC_FLOWRATE_HT;
    else if (strcmp(name, "FLOWRATE_HT_CTRL") == 0)
        result = AC_FLOWRATE_HT_CTRL;
    else if (strcmp(name, "FLOWRATE_MT") == 0)
        result = AC_FLOWRATE_MT;
    else if (strcmp(name, "FLOWRATE_MT_CTRL") == 0)
        result = AC_FLOWRATE_MT_CTRL;
    else if (strcmp(name, "FLOWRATE_LT") == 0)
        result = AC_FLOWRATE_LT;
    else if (strcmp(name, "FLOWRATE_LT_CTRL") == 0)
        result = AC_FLOWRATE_LT_CTRL;
    else if (strcmp(name, "EVAPORATOR_TEMP") == 0)
        result = AC_EVAPORATOR_TEMP;
    else if (strcmp(name, "CONDENSER_TEMP") == 0)
        result = AC_CONDENSER_TEMP;
    else if (strcmp(name, "PHASE_TIME_1") == 0)
        result = AC_PHASE_TIME_1;
    else if (strcmp(name, "PHASE_TIME_2") == 0)
        result = AC_PHASE_TIME_2;
    else if (strcmp(name, "PHASE_TIME_3") == 0)
        result = AC_PHASE_TIME_3;
    else if (strcmp(name, "PHASE_TIME_4") == 0)
        result = AC_PHASE_TIME_4;
    else if (strcmp(name, "PHASE_TIME_5") == 0)
        result = AC_PHASE_TIME_5;
    else if (strcmp(name, "PHASE") == 0)
        result = AC_PHASE;
    else if (strcmp(name, "WORKING_MODE") == 0)
        result = AC_WORKING_MODE;
    else if (strcmp(name, "CHILLER_POWER") == 0)
        result = AC_CHILLER_POWER;
    else if (strcmp(name, "PUMP_POWER") == 0)
        result = AC_PUMP_POWER;

    free(name);
    return result;
}

const char *ads_chiller_metric_str(ads_chiller_metric_type metric)
{
    switch (metric) {
        case AC_IN_TEMP_HT:
            return "IN_TEMP_HT";
        case AC_OUT_TEMP_HT:
            return "OUT_TEMP_HT";
        case AC_IN_TEMP_MT:
            return "IN_TEMP_MT";
        case AC_OUT_TEMP_MT:
            return "OUT_TEMP_MT";
        case AC_IN_TEMP_LT:
            return "IN_TEMP_LT";
        case AC_OUT_TEMP_LT:
            return "OUT_TEMP_LT";
        case AC_VAC_PRESS1:
            return "VAC_PRESS1";
        case AC_VAC_PRESS2:
            return "VAC_PRESS2";
        case AC_FLOWRATE_HT:
            return "FLOWRATE_HT";
        case AC_FLOWRATE_HT_CTRL:
            return "FLOWRATE_HT_CTRL";
        case AC_FLOWRATE_MT:
            return "FLOWRATE_MT";
        case AC_FLOWRATE_MT_CTRL:
            return "FLOWRATE_MT_CTRL";
        case AC_FLOWRATE_LT:
            return "FLOWRATE_LT";
        case AC_FLOWRATE_LT_CTRL:
            return "FLOWRATE_LT_CTRL";
        case AC_EVAPORATOR_TEMP:
            return "EVAPORATOR_TEMP";
        case AC_CONDENSER_TEMP:
            return "CONDENSER_TEMP";
        case AC_PHASE_TIME_1:
            return "PHASE_TIME_1";
        case AC_PHASE_TIME_2:
            return "PHASE_TIME_2";
        case AC_PHASE_TIME_3:
            return "PHASE_TIME_3";
        case AC_PHASE_TIME_4:
            return "PHASE_TIME_4";
        case AC_PHASE_TIME_5:
            return "PHASE_TIME_5";
        case AC_PHASE:
            return "PHASE";
        case AC_WORKING_MODE:
            return "WORKING_MODE";
        case AC_CHILLER_POWER:
            return "CHILLER_POWER";
        case AC_PUMP_POWER:
            return "PUMP_POWER";
        default:
            return "ADS_CHILLER_UNKNOWN_METRIC";
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
    ads_chiller_get_devices(&devs, &devs_count);
    return s;
}

state_t ads_chiller_load(ctx_t *c, int force_api)
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

    ads_chiller_modbus_load(&ops, force_api);
    // Maybe I should just convert inside arch and return std unit here
dummy:
    ads_chiller_dummy_load(&ops);
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

void ads_chiller_get_api(uint *api)
{
    apinfo_t info;
    ads_chiller_get_info(&info);
    *api = info.api;
}

void ads_chiller_get_info(apinfo_t *info)
{
    info->layer       = "ADS_CHILLER";
    info->api         = API_NONE;
    info->devs_count  = 0;
    info->scope       = SCOPE_RACK; // This could also be SCOPE_NODE or SCOPE_CLUSTER
    info->granularity = GRANULARITY_PERIPHERAL;
    if (ops.get_info != NULL) {
        ops.get_info(info);
    }
}

void ads_chiller_get_devices(ads_chiller_devs_t **devs, uint *devs_count)
{
    if (ops.get_devices != NULL) {
        ops.get_devices(devs, devs_count);
    }
}

float ads_chiller_get_metric_mult()
{
    return mult;
}

int ads_chiller_get_is_kelvin()
{
    return is_k;
}

state_t ads_chiller_dispose(ctx_t *c)
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

state_t ads_chiller_read(ctx_t *c, ads_chiller_t *data)
{
    preturn(ops.read, c, data);
}

state_t ads_chiller_read_raw(ctx_t *c, ads_chiller_t *data)
{
    preturn(ops.read_raw, c, data);
}

state_t ads_chiller_read_diff(ctx_t *c, ads_chiller_t *data2, ads_chiller_t *data1, ads_chiller_t *data_diff)
{
    state_t s;
    if (state_fail(s = ads_chiller_read(c, data2))) {
        return s;
    }
    ads_chiller_data_diff(data2, data1, data_diff);
    return s;
}

state_t ads_chiller_read_copy(ctx_t *c, ads_chiller_t *data2, ads_chiller_t *data1, ads_chiller_t *data_diff)
{
    state_t s;
    if (state_fail(s = ads_chiller_read_diff(c, data2, data1, data_diff))) {
        return s;
    }
    ads_chiller_data_copy(data1, data2);
    return s;
}

static void static_data_diff(ads_chiller_t *data2, ads_chiller_t *data1, ads_chiller_t *data_diff, int i)
{
    ads_chiller_t *d3 = &data_diff[i];
    ads_chiller_t *d2 = &data2[i];
    ads_chiller_t *d1 = &data1[i];

    // if (d2->correct != 1 || d1->correct != 1) {
    // 	return;
    // }

    if ((d3->samples = d2->samples - d1->samples) == 0) {
        memset(d3, 0, sizeof(ads_chiller_t));
        return;
    }
    // for (int r = 0; r < d1->num_registers; ++r) {  // I can't use num_registers because
    //  registers order can be different here
    for (int r = 0; r < ADS_CHILLER_MAX_METRIC; ++r) {
        debug("d2 %u d1 %u samples %lu", d2->register_data[r], d1->register_data[r], d3->samples);
        d3->register_data[r] = (d2->register_data[r] - d1->register_data[r]) / d3->samples;
    }
}

void ads_chiller_data_diff(ads_chiller_t *data2, ads_chiller_t *data1, ads_chiller_t *data_diff)
{
    int i;
    ads_chiller_data_null(data_diff);
    for (i = 0; i < devs_count; i++) {
        static_data_diff(data2, data1, data_diff, i);
    }
}

void ads_chiller_data_merge(ads_chiller_t *data_diff, ads_chiller_t *data_merge)
{
    int i;

    memset((void *) data_merge, 0, sizeof(ads_chiller_t));

    for (i = 0; i < devs_count; ++i) {
        for (int r = 0; r < ADS_CHILLER_MAX_METRIC; ++r) {
            data_merge->register_data[r] += data_diff[i].register_data[r];
        }
    }
    // Static
    data_merge->time    = data_diff[0].time;
    data_merge->samples = data_diff[0].samples;
    // Averages
    for (int r = 0; r < ADS_CHILLER_MAX_METRIC; ++r) {
        data_merge->register_data[r] /= devs_count;
    }
}

void ads_chiller_data_alloc(ads_chiller_t **data)
{
    *data = calloc(devs_count, sizeof(ads_chiller_t));
}

void ads_chiller_data_free(ads_chiller_t **data)
{
    free(*data);
    *data = NULL;
}

void ads_chiller_data_null(ads_chiller_t *data)
{
    if (data != NULL) {
        memset(data, 0, devs_count * sizeof(ads_chiller_t));
    }
}

void ads_chiller_data_copy(ads_chiller_t *data_dst, ads_chiller_t *data_src)
{
    memcpy(data_dst, data_src, devs_count * sizeof(ads_chiller_t));
}

void ads_chiller_data_print(ads_chiller_t *data, int fd)
{
    int i;
    for (i = 0; i < devs_count; ++i) {
        dprintf(fd, "ads chiller %u: %4u C, %4u C, %4u C, %4u C, %4u C, %4u C, \
			%4u l/s, %4u l/s, %4u l/s, %4u C, %4u C, %4u kPa, %4u kPa, %4lu\n",
                i, data[i].register_data[AC_IN_TEMP_HT], data[i].register_data[AC_IN_TEMP_MT],
                data[i].register_data[AC_IN_TEMP_LT], data[i].register_data[AC_OUT_TEMP_HT],
                data[i].register_data[AC_OUT_TEMP_MT], data[i].register_data[AC_OUT_TEMP_LT],
                data[i].register_data[AC_FLOWRATE_HT], data[i].register_data[AC_FLOWRATE_MT],
                data[i].register_data[AC_FLOWRATE_LT], data[i].register_data[AC_EVAPORATOR_TEMP],
                data[i].register_data[AC_CONDENSER_TEMP], data[i].register_data[AC_VAC_PRESS1],
                data[i].register_data[AC_VAC_PRESS2], data[i].samples);
    }
}

char *ads_chiller_data_tostr(ads_chiller_t *data, char *buffer, int length)
{
    int accuml;
    size_t s;
    int i;

    for (i = accuml = 0; i < devs_count && length > 0; ++i) {
        s      = snprintf(&buffer[accuml], length, "ads chiller %u: %4u C, %4u C, %4u C, %4u C, %4u C, %4u C, \
			%4u l/s, %4u l/s, %4u l/s, %4u C, %4u C, %4u kPa, %4u kPa, %4lu\n",
                          i, data[i].register_data[AC_IN_TEMP_HT], data[i].register_data[AC_IN_TEMP_MT],
                          data[i].register_data[AC_IN_TEMP_LT], data[i].register_data[AC_OUT_TEMP_HT],
                          data[i].register_data[AC_OUT_TEMP_MT], data[i].register_data[AC_OUT_TEMP_LT],
                          data[i].register_data[AC_FLOWRATE_HT], data[i].register_data[AC_FLOWRATE_MT],
                          data[i].register_data[AC_FLOWRATE_LT], data[i].register_data[AC_EVAPORATOR_TEMP],
                          data[i].register_data[AC_CONDENSER_TEMP], data[i].register_data[AC_VAC_PRESS1],
                          data[i].register_data[AC_VAC_PRESS2], data[i].samples);
        length = length - s;
        accuml = accuml + s;
    }
    return buffer;
}

double ads_chiller_get_metric(ads_chiller_t *data, ads_chiller_metric_type metric)
{
    if (metric >= ADS_CHILLER_MAX_METRIC || data == NULL) {
        return -1.0;
    }
    double value = (double) data->register_data[metric];

    if (mult <= 0)
        mult = 1.0;

    // Power metrics are unscaled direct Watts -- no multiplier, no Kelvin offset.
    if (metric == AC_CHILLER_POWER || metric == AC_PUMP_POWER) {
        return value;
    }

    if ((metric >= AC_IN_TEMP_HT && metric <= AC_OUT_TEMP_LT) || metric == AC_EVAPORATOR_TEMP ||
        metric == AC_CONDENSER_TEMP) {
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

int ads_chiller_is_supported()
{
    // return ads_chiller_eard_is_supported();
    return 1;
}
