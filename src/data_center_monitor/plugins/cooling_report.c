/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#define SHOW_DEBUGS  1
#define VERBOSE_DATA 0

#include <string.h>

#include <common/config.h>
#include <common/output/debug.h>
#include <common/output/verbose.h>
#include <common/system/plugin_manager.h>
#include <common/utils/string.h>
#include <data_center_monitor/plugins/conf.h>
#include <data_center_monitor/plugins/cooling_monitor.h>
#include <report/report.h>

#define DEFAULT_REPORT_PLUGIN "cooling_csv.so"

static cooling_data_t *cooling_info;
// Report
static conf_t *conf;
static report_id_t rid;

static char my_report_plugin[128];

declr_up_get_tag()
{
    *tag = "cooling_report";
    // To be executed with the same period than nodesensors
    *tags_deps = "!conf+<!cooling_monitor";
}

declr_up_action_init(_cooling_report)
{
    // data is the list of arguments for this plugin. It is received in the format of argv (null terminated)
    // In this case is the specific report plugin
    char **args = (char **) data;
    state_t st;

    if (args && args[0]) {
        verbose(0, "Initializing cooling report plug ini argc received %s", args[0]);
        strcpy(my_report_plugin, args[0]);
        strcat(my_report_plugin, ".so");
    } else {
        verbose(0, "No cooling report plugin specified, using default");
        strcpy(my_report_plugin, DEFAULT_REPORT_PLUGIN);
    }

    verbose(0, "Using %s report plugin. Plug path %s", my_report_plugin, conf->cluster.install.dir_plug);
    st = report_load(conf->cluster.install.dir_plug, my_report_plugin);
    if (state_fail(st) && (ear_getenv(ENV_PATH_EAR) != NULL)) {
        char install_dir_plug[1024];
        snprintf(install_dir_plug, sizeof(install_dir_plug), "%s/%s", ear_getenv(ENV_PATH_EAR), "/lib/plugins");
        verbose(0, "Using %s report plugin. Plug path %s", my_report_plugin, install_dir_plug);
        st = report_load(install_dir_plug, my_report_plugin);
    }
    if (state_ok(st)) {

        report_create_id(&rid, 0, 0, 0);
        // Initializing report manager
        if (state_fail(report_init(&rid, &conf->cluster))) {
            return rsprintf("[D] cooling_report plugin cannot be loaded");
        }
    } else {
        return rsprintf("[D] cooling_report plugin cannot be loaded");
    }
    verbose(0, "cooling_report report plugin loaded");

    return rsprintf("cooling_report action init(): received tag %s ", tag);
}

declr_up_action_init(_conf)
{
    conf = (conf_t *) data;

    return rsprintf("cooling_report action init(_conf): received tag %s", tag);
}

declr_up_action_init(_cooling_monitor)
{
    cooling_info = (cooling_data_t *) data;

    return rsprintf("cooling_report action init(_cooling_mon): received tag %s", tag);
}

// This action will be executed after cooling action
declr_up_action_periodic(_cooling_monitor)
{
    debug("cooling_report action tag %s", tag);
#if VERBOSE_DATA
    char cooling_str[1024];
    time_tostr(coolinnfo->timestamp, cooling_str, sizeof(cooling_str));
    verbose(VERBOSE_DATA,
            "Cooling report at %s: type %s, dev_id %llu, num_in_temp_ht %u, num_out_temp_ht %u, "
            "num_flowrate %u, num_power %u",
            cooling_str, pdu_type_to_str(cooling_info->type), cooling_info->dev_id, cooling_info->num_in_temp_ht,
            cooling_info->num_out_temp_ht, cooling_info->num_flowrate, cooling_info->num_power);
    for (uint p = 0; p < cooling_info->num_in_temp_ht; p++) {
        verbose(VERBOSE_DATA, "  in_temp_ht[%u]: name %s, value %f", p, cooling_info->in_temp_ht[p].name,
                cooling_info->in_temp_ht[p].value);
    }
    for (uint t = 0; t < cooling_info->num_out_temp_ht; t++) {
        verbose(VERBOSE_DATA, "  out_temp_ht[%u]: name %s, value %f", t, cooling_info->out_temp_ht[t].name,
                cooling_info->out_temp_ht[t].value);
    }
    for (uint f = 0; f < cooling_info->num_flowrate; f++) {
        verbose(VERBOSE_DATA, "  flowrate[%u]: name %s, value %f", f, cooling_info->flowrate[f].name,
                cooling_info->flowrate[f].value);
    }
    for (uint p = 0; p < cooling_info->num_power; p++) {
        verbose(VERBOSE_DATA, "  power[%u]: name %s, value %f", p, cooling_info->power[p].name,
                cooling_info->power[p].value);
    }
#endif
    report_misc(&rid, COOLING_MON_TYPE, (const char *) cooling_info, 1);
    return rsprintf("cooling_reports action periodic(_cooling_monitor): received tag %s", tag);
}

declr_up_action_close()
{
    debug("DCMON: cooling report close");
    report_dispose(&rid);
    return rsprintf("cooling_report action close()");
}