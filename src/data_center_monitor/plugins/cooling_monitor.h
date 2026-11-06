/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef _COOLING_MON_H_
#define _COOLING_MON_H_

// SINGLE_NODE is a build-time flag (see Makefile): when set, the IT power associated with the
// cooling devices is read from the local node (metrics API); otherwise it is taken from EARGM.

#include <common/config.h>
#include <common/states.h>
#include <common/system/plugin_manager.h>
#include <common/types/configuration/cluster_conf_edcmon_tag.h>
#include <common/types/generic.h>
#include <sys/types.h>
#include <time.h>

#define MAX_SOURCE_DESC_SIZE 128
#define MAX_METRICS          32
#define MAX_ADS_CHILLERS     1
#define MAX_TWO_PHASES       1

typedef struct temp_metric {
    double value;
    // float coeff; //default unit here, so I don't need coeff and constant
    // float constant;
    char name[MAX_SOURCE_DESC_SIZE];
} temp_metric_t;

// Thermal and power data are the same for now
typedef temp_metric_t pressure_metric_t;
typedef temp_metric_t flowrate_metric_t;
typedef temp_metric_t power_metric_t;

typedef struct cooling_data {
    time_t timestamp;
    temp_metric_t in_temp_ht[MAX_ADS_CHILLERS + MAX_TWO_PHASES];
    uint num_in_temp_ht;
    temp_metric_t out_temp_ht[MAX_ADS_CHILLERS + MAX_TWO_PHASES];
    uint num_out_temp_ht;
    flowrate_metric_t flowrate[MAX_ADS_CHILLERS + MAX_TWO_PHASES];
    uint num_flowrate;
    power_metric_t power[MAX_ADS_CHILLERS + MAX_TWO_PHASES];
    uint num_power;
    // ADS chiller-only loops (two_phase devices have no MT/LT/evaporator/condenser):
    // indexed 0..num_ads_chiller_loops-1.
    temp_metric_t in_temp_mt[MAX_ADS_CHILLERS];
    temp_metric_t out_temp_mt[MAX_ADS_CHILLERS];
    temp_metric_t in_temp_lt[MAX_ADS_CHILLERS];
    temp_metric_t out_temp_lt[MAX_ADS_CHILLERS];
    temp_metric_t evaporator_temp[MAX_ADS_CHILLERS];
    temp_metric_t condenser_temp[MAX_ADS_CHILLERS];
    // MT/LT loop flowrates. `flowrate[]` above is the HT loop only. Each loop has its own pump,
    // so these are generally NOT equal to the HT flowrate.
    flowrate_metric_t flowrate_mt[MAX_ADS_CHILLERS];
    flowrate_metric_t flowrate_lt[MAX_ADS_CHILLERS];
    // The cooling plant's own electrical draw, as reported by the device (AC_CHILLER_POWER / AC_PUMP_POWER).
    power_metric_t chiller_power[MAX_ADS_CHILLERS];
    power_metric_t pump_power[MAX_ADS_CHILLERS];
    uint num_ads_chiller_loops;
    temp_metric_t dev_temp[MAX_METRICS];
    uint num_dev_temp;
    double max_dev_temp[MAX_ADS_CHILLERS + MAX_TWO_PHASES];
    ulong total_power;

    pdu_t type;
    ulong dev_id;

} cooling_data_t;

declr_up_get_tag();
declr_up_action_init(_conf);
declr_up_action_init(_cooling_monitor);
declr_up_action_periodic(_cooling_monitor);

#endif // _COOLING_MON_H_
