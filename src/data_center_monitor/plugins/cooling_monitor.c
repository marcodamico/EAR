/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#define SHOW_DEBUGS        1
#define COOLING_VLV        0
#define DEFAULT_UNIT_SCALE 1

#include <common/config.h>
#include <common/includes.h>
#include <common/output/debug.h>
#include <common/states.h>
#include <common/system/plugin_manager.h>
#include <common/types.h>
#include <common/types/configuration/cluster_conf_edcmon_tag.h>
#include <daemon/local_api/eard_api.h>

#include <metrics/ads_chiller/ads_chiller.h>
#include <metrics/two_phase/two_phase.h>

#include <data_center_monitor/plugins/conf.h>
#include <data_center_monitor/plugins/cooling_monitor.h>

#ifdef SINGLE_NODE
#include <metrics/metrics.h>
#else
#include <global_manager/eargm_rapi.h>
#endif

// EDCMON configuration : list of pdus, sensors, etc
static char my_hostname[128];
static conf_t *conf;
static uint total_cooling_devices;
static cooling_data_t cooling_info;
static ads_chiller_t *ac1, *ac2, *acD;
static two_phase_t *tf1, *tf2, *tfD;
static uint num_ads_chillers, num_two_phases;
static ads_chiller_devs_t *ads_chillers = NULL;
static two_phase_devs_t *two_phases     = NULL;

#ifdef SINGLE_NODE
static char *np_path = NULL; // Node power
static char buffer[4096];
static char buffer1[4096];
static metrics_read_t mr1;
static metrics_read_t mr2;
static metrics_diff_t mrD;
static metrics_info_t mi;
static uint num_gpus;
static uint met_options_eard[MET_OPT_MAX];
#else
// static eargm_def_t *e_def;
static int eargm_idx = -1;
static eargm_status_t *eargm_statuses;
static char **eargm_node   = NULL;
static int eargm_num_nodes = 0;
#endif
static ulong current_power = 0;

declr_up_get_tag()
{
    *tag       = "cooling_monitor";
    *tags_deps = "!conf";
}

static void cooling_static_init()
{
    // Root is not needed to read the cooling devices through Modbus, but it is needed to read
    // the node metrics locally; otherwise they are requested to EARD.
    ullong mflags = API_FREE;
#ifdef SINGLE_NODE
    uint *met_options = NULL;
    if (getuid() != 0) {
        if (state_fail(eards_connection())) {
            error("EARD cannot be connected");
            return;
        }
        for (uint i = 0; i < MET_OPT_MAX; i++) {
            met_options_eard[i] = API_EARD;
        }
        met_options = met_options_eard;
    }
#endif

    debug("Metrics initialization ");
    ctx_t *ctx   = (ctx_t *) malloc(sizeof(ctx_t));
    ctx->context = conf;
    ads_chiller_load(ctx, mflags);
    two_phase_load(ctx, mflags);

#ifdef SINGLE_NODE
    verbose(0, "Single node mode, using metrics interface");
    metrics_load(&mi, &conf->tp, np_path, met_options);

    metrics_info_tostr(&mi, buffer1);
    verbose(COOLING_VLV, "%s", buffer1);

    metrics_data_alloc(&mr1, &mr2, &mrD);
    metrics_read(&mr1);

    gpu_get_devices(NULL, &num_gpus);

    // TODO: the reporting of node info?
#else
    verbose(0, "Multiple nodes mode, using EARGM");
    // Use EARGM
#endif

    ads_chiller_get_devices(&ads_chillers, &num_ads_chillers);
    two_phase_get_devices(&two_phases, &num_two_phases);

    ads_chiller_data_alloc(&ac1);
    ads_chiller_data_alloc(&ac2);
    ads_chiller_data_alloc(&acD);

    two_phase_data_alloc(&tf1);
    two_phase_data_alloc(&tf2);
    two_phase_data_alloc(&tfD);

    ads_chiller_read(NULL, ac1);
    two_phase_read(NULL, tf1);

    if (num_ads_chillers == 0) {
        verbose(0, "No ADS Chiller found");
        // return;
    }
    if (num_two_phases == 0) {
        verbose(0, "No Two Phase found");
        // return;
    }
    if (num_ads_chillers > MAX_ADS_CHILLERS) {
        debug("Too many ADS Chillers, only %d are supported", MAX_ADS_CHILLERS);
    }
    if (num_two_phases > MAX_TWO_PHASES) {
        debug("Too many Two Phase, only %d are supported", MAX_TWO_PHASES);
    }

    total_cooling_devices += num_ads_chillers + num_two_phases;

    for (uint i = 0; i < num_ads_chillers; i++) {
        cooling_info.in_temp_ht[i].value = 0;
        snprintf(cooling_info.in_temp_ht[i].name, sizeof(cooling_info.in_temp_ht[i].name), "in_temp_ht");
    }
    for (uint i = 0; i < num_two_phases; i++) {
        cooling_info.in_temp_ht[i + num_ads_chillers].value = 0;
        snprintf(cooling_info.in_temp_ht[i + num_ads_chillers].name,
                 sizeof(cooling_info.in_temp_ht[i + num_ads_chillers].name), "in_temp_ht");
    }

    for (uint i = 0; i < num_ads_chillers; i++) {
        cooling_info.out_temp_ht[i].value = 0;
        snprintf(cooling_info.out_temp_ht[i].name, sizeof(cooling_info.out_temp_ht[i].name), "out_temp_ht");
    }
    for (uint i = 0; i < num_two_phases; i++) {
        cooling_info.out_temp_ht[i + num_ads_chillers].value = 0;
        snprintf(cooling_info.out_temp_ht[i + num_ads_chillers].name,
                 sizeof(cooling_info.out_temp_ht[i + num_ads_chillers].name), "out_temp_ht");
    }
    for (uint i = 0; i < num_ads_chillers; i++) {
        cooling_info.flowrate[i].value = 0;
        snprintf(cooling_info.flowrate[i].name, sizeof(cooling_info.flowrate[i].name), "flowrate");
    }
    for (uint i = 0; i < num_two_phases; i++) {
        cooling_info.flowrate[i + num_ads_chillers].value = 0;
        snprintf(cooling_info.flowrate[i + num_ads_chillers].name,
                 sizeof(cooling_info.flowrate[i + num_ads_chillers].name), "flowrate");
    }

    for (uint i = 0; i < total_cooling_devices; i++) {
        cooling_info.power[i].value = 0;
        snprintf(cooling_info.power[i].name, sizeof(cooling_info.power[i].name), "controlled_power");
    }

    // ADS chiller-only loops: two_phase devices have no MT/LT/evaporator/condenser.
    for (uint i = 0; i < num_ads_chillers; i++) {
        cooling_info.in_temp_mt[i].value = 0;
        snprintf(cooling_info.in_temp_mt[i].name, sizeof(cooling_info.in_temp_mt[i].name), "in_temp_mt");
        cooling_info.out_temp_mt[i].value = 0;
        snprintf(cooling_info.out_temp_mt[i].name, sizeof(cooling_info.out_temp_mt[i].name), "out_temp_mt");
        cooling_info.in_temp_lt[i].value = 0;
        snprintf(cooling_info.in_temp_lt[i].name, sizeof(cooling_info.in_temp_lt[i].name), "in_temp_lt");
        cooling_info.out_temp_lt[i].value = 0;
        snprintf(cooling_info.out_temp_lt[i].name, sizeof(cooling_info.out_temp_lt[i].name), "out_temp_lt");
        cooling_info.evaporator_temp[i].value = 0;
        snprintf(cooling_info.evaporator_temp[i].name, sizeof(cooling_info.evaporator_temp[i].name), "evaporator_temp");
        cooling_info.condenser_temp[i].value = 0;
        snprintf(cooling_info.condenser_temp[i].name, sizeof(cooling_info.condenser_temp[i].name), "condenser_temp");
        cooling_info.flowrate_mt[i].value = 0;
        snprintf(cooling_info.flowrate_mt[i].name, sizeof(cooling_info.flowrate_mt[i].name), "flowrate_mt");
        cooling_info.flowrate_lt[i].value = 0;
        snprintf(cooling_info.flowrate_lt[i].name, sizeof(cooling_info.flowrate_lt[i].name), "flowrate_lt");
        cooling_info.chiller_power[i].value = 0;
        snprintf(cooling_info.chiller_power[i].name, sizeof(cooling_info.chiller_power[i].name), "chiller_power");
        cooling_info.pump_power[i].value = 0;
        snprintf(cooling_info.pump_power[i].name, sizeof(cooling_info.pump_power[i].name), "pump_power");
    }
    cooling_info.num_ads_chiller_loops = num_ads_chillers;

    cooling_info.num_in_temp_ht  = total_cooling_devices;
    cooling_info.num_out_temp_ht = total_cooling_devices;
    cooling_info.num_flowrate    = total_cooling_devices;
    cooling_info.num_power       = total_cooling_devices;

    // TODO doesnt make sense right now
    for (uint s = 0; s < num_ads_chillers; s++) {
        cooling_info.type   = ads_chiller;
        cooling_info.dev_id = ads_chillers[s].serial;
    }

    // for (uint s = 0; s < num_two_phases; s++) {
    //     cooling_info.type   = two_phase;
    //     cooling_info.dev_id = two_phases[s].serial;
    // }

    free(ctx);
    debug("Cooling_monitor Initialization finished");
}

declr_up_action_init(_cooling_monitor)
{
    // We expose this information
    *data_alloc = &cooling_info;
    debug("DCMON: cooling monitor initialization");
    cooling_static_init();
    return rsprintf("cooling_monitor action init(_cooling_monitor): received tag %s", tag);
}

/* We use the configuration from conf plugin (ear.conf) */
declr_up_action_init(_conf)
{
    conf = (conf_t *) data;

    // TODO: verify I am the correct host running this service

    debug("Initializing conf tag");
    // We get my name to look for my queries
    if (gethostname(my_hostname, sizeof(my_hostname)) < 0) {
        error("Error getting node name (%s)", strerror(errno));
    }

    strtok(my_hostname, ".");

    debug("Server name is %s", my_hostname);

#ifdef SINGLE_NODE
    if (getuid() != 0) {
        sprintf(buffer, "%s/energy/energy_eard_nm_local_api.so", conf->cluster.install.dir_plug);
    } else {
        sprintf(buffer, "%s/energy/%s", conf->cluster.install.dir_plug, conf->cluster.install.obj_ener);
    }
    np_path = buffer;

    debug("Using Node energy reading plugin: %s", np_path);
#else
    // TODO: In two-phase this could be even more fine tuned

    // Old implementation, works if eargm and cooling monitor are in the same node
    // e_def = get_eargm_conf(&conf->cluster, my_hostname);
    // if (e_def == NULL) {
    //     error("Could not get configuration");
    // }

    // for (int i = 0; i < conf->cluster.eargm.num_eargms; i++) {
    //     debug("EARGM[%d]: %s:%u", i, conf->cluster.eargm.eargms[i].node, conf->cluster.eargm.eargms[i].port);
    //     if (conf->cluster.eargm.eargms[i].id == e_def->id) {
    //         eargm_idx = i;
    //         break;
    //     }
    // }

    // TODO: Just take the first for now, in the future multiple EARGM could be supported
    for (int i = 0; i < conf->cluster.num_edcmons_tags; i++) {
        debug("EDCMON[%d]: %s (EARGM id: %d)", i, conf->cluster.edcmon_tags[i].id,
              conf->cluster.edcmon_tags[i].eargm_id);
        if (conf->cluster.edcmon_tags[i].eargm_id > 0)
            eargm_idx = conf->cluster.edcmon_tags[i].eargm_id;
    }

    if (eargm_idx < 0) {
        error("EARGM is not defined in the configuration file (ear.conf)");
    }
#endif
    return rsprintf("cooling_monitor action init(_conf): received tag %s", tag);
}

static void from_metrics_to_cooling(ads_chiller_t *Diff, cooling_data_t *th)
{
    debug("from_metrics_to_cooling");
    th->timestamp = time(NULL);
    debug("Set timestamp to %lu", th->timestamp);

    debug("Processing %u ADS chillers and %u two-phase", num_ads_chillers, num_two_phases);

    for (uint s = 0; s < num_ads_chillers; s++) {
        th->in_temp_ht[s].value = ads_chiller_get_metric(&Diff[s], AC_IN_TEMP_HT);
        debug("Chiller %u: in_temp_ht = %lf", s, th->in_temp_ht[s].value);
    }
    for (uint s = 0; s < num_ads_chillers; s++) {
        th->out_temp_ht[s].value = ads_chiller_get_metric(&Diff[s], AC_OUT_TEMP_HT);
        debug("Chiller %u: out_temp_ht = %lf", s, th->out_temp_ht[s].value);
    }
    for (uint s = 0; s < num_ads_chillers; s++) {
        th->flowrate[s].value = ads_chiller_get_metric(&Diff[s], AC_FLOWRATE_HT);
        debug("Chiller %u: flowrate = %lf", s, th->flowrate[s].value);
    }

    // MT (condenser/cooling-water) and LT (chilled-water-to-load) loops, plus
    // evaporator/condenser temps.
    for (uint s = 0; s < num_ads_chillers; s++) {
        th->in_temp_mt[s].value  = ads_chiller_get_metric(&Diff[s], AC_IN_TEMP_MT);
        th->out_temp_mt[s].value = ads_chiller_get_metric(&Diff[s], AC_OUT_TEMP_MT);
        th->in_temp_lt[s].value  = ads_chiller_get_metric(&Diff[s], AC_IN_TEMP_LT);
        th->out_temp_lt[s].value = ads_chiller_get_metric(&Diff[s], AC_OUT_TEMP_LT);
        debug("Chiller %u: in_temp_mt = %lf, out_temp_mt = %lf, in_temp_lt = %lf, out_temp_lt = %lf", s,
              th->in_temp_mt[s].value, th->out_temp_mt[s].value, th->in_temp_lt[s].value, th->out_temp_lt[s].value);
    }
    for (uint s = 0; s < num_ads_chillers; s++) {
        th->evaporator_temp[s].value = ads_chiller_get_metric(&Diff[s], AC_EVAPORATOR_TEMP);
        th->condenser_temp[s].value  = ads_chiller_get_metric(&Diff[s], AC_CONDENSER_TEMP);
        debug("Chiller %u: evaporator_temp = %lf, condenser_temp = %lf", s, th->evaporator_temp[s].value,
              th->condenser_temp[s].value);
    }
    // MT/LT loop flowrates. Each loop has its own pump and is generally NOT at the HT flowrate.
    for (uint s = 0; s < num_ads_chillers; s++) {
        th->flowrate_mt[s].value = ads_chiller_get_metric(&Diff[s], AC_FLOWRATE_MT);
        th->flowrate_lt[s].value = ads_chiller_get_metric(&Diff[s], AC_FLOWRATE_LT);
        debug("Chiller %u: flowrate_mt = %lf, flowrate_lt = %lf", s, th->flowrate_mt[s].value,
              th->flowrate_lt[s].value);
    }
    // The plant's own electrical draw.
    for (uint s = 0; s < num_ads_chillers; s++) {
        th->chiller_power[s].value = ads_chiller_get_metric(&Diff[s], AC_CHILLER_POWER);
        th->pump_power[s].value    = ads_chiller_get_metric(&Diff[s], AC_PUMP_POWER);
        debug("Chiller %u: chiller_power = %lf W, pump_power = %lf W", s, th->chiller_power[s].value,
              th->pump_power[s].value);
    }
    th->num_ads_chiller_loops = num_ads_chillers;

    for (uint s = 0; s < num_two_phases; s++) {
        uint base                   = s + num_ads_chillers;
        th->in_temp_ht[base].value  = two_phase_get_metric(&tfD[s], TP_IN_TEMP_HT);
        th->out_temp_ht[base].value = two_phase_get_metric(&tfD[s], TP_OUT_TEMP_HT);
        th->flowrate[base].value    = two_phase_get_metric(&tfD[s], TP_FLOWRATE_HT);
    }

    debug("from_metrics_to_cooling completed successfully");
}

#ifdef SINGLE_NODE
static void from_single_node_metrics_to_cooling(metrics_read_t *mr, cooling_data_t *th)
{
    uint i;
    double max_t     = 0.0;
    th->num_dev_temp = 0;
    // CPU temperatures
    for (i = 0; i < mi.temp.devs_count && th->num_dev_temp < MAX_METRICS; i++) {
        double val                           = (double) mr->temp[i];
        th->dev_temp[th->num_dev_temp].value = val;
        if (val > max_t)
            max_t = val;
        snprintf(th->dev_temp[th->num_dev_temp].name, sizeof(th->dev_temp[th->num_dev_temp].name), "CPU_%u", i);
        th->num_dev_temp++;
    }
    // GPU temperatures
    for (i = 0; i < num_gpus && th->num_dev_temp < MAX_METRICS; i++) {
        double val                           = (double) mr->gpu[i].temp_gpu;
        th->dev_temp[th->num_dev_temp].value = val;
        if (val > max_t)
            max_t = val;
        snprintf(th->dev_temp[th->num_dev_temp].name, sizeof(th->dev_temp[th->num_dev_temp].name), "GPU_%u", i);
        th->num_dev_temp++;
    }
    th->max_dev_temp[0] = max_t; // In single node, everything belongs to chiller 0
    debug("Populated %u device temperatures, max_dev_temp[0] = %.2f", th->num_dev_temp, max_t);
}
#else

static void from_eargm_metrics_to_cooling(eargm_status_t *status, int num_status, cooling_data_t *th)
{
    // TODO: When EARGM supports temperature reporting, populate th->dev_temp here
    th->num_dev_temp = 0;
    debug("EARGM case: temperatures not yet supported");
}
#endif

declr_up_action_periodic(_cooling_monitor)
{
    debug("DCMON: cooling monitor action");

    debug("Reading metrics for period");
    if (ads_chiller_read(NULL, ac2) != EAR_SUCCESS) {
        error("Error reading metrics");
        // return rsprintf("cooling_monitor action periodic(_cooling_monitor): received tag %s", tag);
    }

    // The register pools ACCUMULATE raw registers across reads (`+=` in the modbus pool
    // threads) and count the reads in `samples`; only the diff -- *_data_diff(), which divides
    // by the sample delta -- carries physical values. ac2/tf2 on their own grow without bound.
    ads_chiller_data_diff(ac2, ac1, acD);
#if 0
	debug("Metrics computed");
	verbose(COOLING_VLV, "%s", metrics_data_tostr(&mrD));
#endif
    // TODO: Why not working with pointers?
    ads_chiller_data_copy(ac1, ac2);

    if (num_two_phases > 0) {
        if (two_phase_read(NULL, tf2) != EAR_SUCCESS) {
            error("Error reading two phase metrics");
        }
        two_phase_data_diff(tf2, tf1, tfD);
        two_phase_data_copy(tf1, tf2);
    }

    debug("Converting to Cooling data");
    from_metrics_to_cooling(acD, &cooling_info);

#ifdef SINGLE_NODE
    metrics_read(&mr2);
    metrics_data_diff(&mr2, &mr1, &mrD);

    current_power = mrD.nodepow_avrg / mrD.time;
    debug("nodavg %lu time %lf", mrD.nodepow_avrg, mrD.time);

    metrics_data_copy(&mr1, &mr2);

    from_single_node_metrics_to_cooling(&mr1, &cooling_info);

#else
    int num_status;
    // num_status = eargm_get_all_status(&conf->cluster, &status, eargm_idx);
    num_status = eargm_get_status(&conf->cluster, &eargm_statuses, eargm_node, eargm_num_nodes);
    debug("Received %d status", num_status);
    current_power = 0;

    for (int i = 0; i < num_status; i++) {

        if (eargm_statuses[i].id <= 0) {
            error("EARGM status[%d] has invalid id: %d", i, eargm_statuses[i].id);
        }
        debug("EARGM status[%d]: id=%d, status=%c, def_power=%lu, current_power=%lu, current_powercap=%lu, "
              "requested=%lu, freeable_power=%lu, extra_power=%lu, energy=%lu",
              i, eargm_statuses[i].id, eargm_statuses[i].status, eargm_statuses[i].def_power,
              eargm_statuses[i].current_power, eargm_statuses[i].current_powercap, eargm_statuses[i].requested,
              eargm_statuses[i].freeable_power, eargm_statuses[i].extra_power, eargm_statuses[i].energy);

        if (eargm_statuses[i].id == eargm_idx) {
            current_power = eargm_statuses[i].current_power;
            if (current_power == 0) {
                error("EARGM status[%d] current_power is zero", i);
            }
        }
    }
    from_eargm_metrics_to_cooling(eargm_statuses, num_status, &cooling_info);
#endif
    debug("Current power: %lu", current_power);

    cooling_info.total_power = current_power;
    // IT power carried by each cooling device. Today every device covers the whole monitored
    // domain (the node in SINGLE_NODE, the EARGM otherwise), so they all report the same value.
    for (uint i = 0; i < cooling_info.num_power; i++) {
        cooling_info.power[i].value = (double) current_power;
    }
    return rsprintf("cooling_monitor action periodic(_cooling_monitor): received tag %s", tag);
}

declr_up_action_close()
{
    debug("DCMON: cooling monitor close");
    ads_chiller_dispose(NULL);
    two_phase_dispose(NULL);
    if (ads_chillers != NULL) {
        free(ads_chillers);
        ads_chillers = NULL;
    }
    if (two_phases != NULL) {
        free(two_phases);
        two_phases = NULL;
    }
    return rsprintf("cooling_monitor action close()");
}

#if TEST
void main(int argc, char *argv[])
{
    uint num_items;
    static_init();
    static_cooling_mon();
    print_cooling_data();
}
#endif