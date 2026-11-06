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
#include <common/config.h>
#define _GNU_SOURCE /* See feature_test_macros(7) */
#include <errno.h>
#include <fcntl.h> /* Definition of AT_* constants */
#include <stdio.h>
#include <sys/stat.h>

extern char *program_invocation_name;
extern char *program_invocation_short_name;

#include <common/config.h>
#include <common/output/verbose.h>
#include <common/states.h>
#include <common/types/configuration/cluster_conf.h>
#include <common/types/types.h>
#include <data_center_monitor/plugins/cooling_monitor.h>
#include <report/report.h>

static char log_file_name_cooling[SZ_PATH];
static char nodename[256];

static int fd_cooling = -1;

static uint must_report = 1;
static int first_time   = 1;

state_t report_init(report_id_t *id, cluster_conf_t *cconf)
{
    if (gethostname(nodename, sizeof(nodename)) < 0) {
        sprintf(nodename, "noname");
    }
    strtok(nodename, ".");

    sprintf(log_file_name_cooling, "%s/%s.%s.cooling.csv", cconf->install.dir_temp, program_invocation_short_name,
            nodename);

    debug("Using cooling log file %s", log_file_name_cooling);

    // First try to open without O_CREAT to check if file exists
    fd_cooling = open(log_file_name_cooling, O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IROTH);
    if (fd_cooling >= 0) {
        first_time = 0;
    } else {
        // File doesn't exist, create it
        fd_cooling = open(log_file_name_cooling, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IROTH);
    }

    if (fd_cooling < 0) {
        debug("Error creating files, log plugin disabled");
        must_report = 0;
    }

    // chmod(log_file_name_cooling, S_IRUSR|S_IWUSR|S_IROTH);

    return EAR_SUCCESS;
}

state_t report_misc(report_id_t *id, uint type, const char *data_in, uint count)
{
    if (!must_report)
        return EAR_SUCCESS;

    if (type != COOLING_MON_TYPE)
        return EAR_SUCCESS;

    cooling_data_t *cooling_info = (cooling_data_t *) data_in;
    if (count > 1) {
        verbose(0, "WARNING: cooling log only supports 1 item at a time");
    }
    // Can data format change and then needs to be updated here? Or once initialized it will be the same?
    // For now assume it is the same
    if (first_time) {
        // Print header of the CSV file
        dprintf(fd_cooling, "timestamp");
        dprintf(fd_cooling, ",dev_type");
        dprintf(fd_cooling, ",dev_id");
        for (uint p = 0; p < cooling_info->num_in_temp_ht; p++) {
            dprintf(fd_cooling, ",");
            dprintf(fd_cooling, "%s", cooling_info->in_temp_ht[p].name);
        }

        for (uint t = 0; t < cooling_info->num_out_temp_ht; t++) {
            dprintf(fd_cooling, ",");
            dprintf(fd_cooling, "%s", cooling_info->out_temp_ht[t].name);
        }

        for (uint f = 0; f < cooling_info->num_flowrate; f++) {
            dprintf(fd_cooling, ",");
            dprintf(fd_cooling, "%s", cooling_info->flowrate[f].name);
        }
        for (uint p = 0; p < cooling_info->num_power; p++) {
            dprintf(fd_cooling, ",");
            dprintf(fd_cooling, "%s", cooling_info->power[p].name);
        }
        dprintf(fd_cooling, ",total_power");

        // ADS chiller-only loops.
        for (uint s = 0; s < cooling_info->num_ads_chiller_loops; s++) {
            dprintf(fd_cooling, ",%s,%s,%s,%s,%s,%s,chiller_power_%u,pump_power_%u,flowrate_mt_%u,flowrate_lt_%u",
                    cooling_info->in_temp_mt[s].name, cooling_info->out_temp_mt[s].name,
                    cooling_info->in_temp_lt[s].name, cooling_info->out_temp_lt[s].name,
                    cooling_info->evaporator_temp[s].name, cooling_info->condenser_temp[s].name, s, s, s, s);
        }
        for (uint s = 0; s < cooling_info->num_flowrate; s++) {
            dprintf(fd_cooling, ",max_dev_temp_%u", s);
        }

        dprintf(fd_cooling, "\n");
        first_time = 0;
    }

    dprintf(fd_cooling, "%ld", cooling_info->timestamp);
    dprintf(fd_cooling, ",%s", pdu_type_to_str(cooling_info->type));
    dprintf(fd_cooling, ",%lu", cooling_info->dev_id);

    for (uint p = 0; p < cooling_info->num_in_temp_ht; p++) {
        dprintf(fd_cooling, ",");
        dprintf(fd_cooling, "%f", (float) cooling_info->in_temp_ht[p].value);
    }
    for (uint t = 0; t < cooling_info->num_out_temp_ht; t++) {
        dprintf(fd_cooling, ",");
        dprintf(fd_cooling, "%f", (float) cooling_info->out_temp_ht[t].value);
    }
    for (uint f = 0; f < cooling_info->num_flowrate; f++) {
        dprintf(fd_cooling, ",");
        dprintf(fd_cooling, "%f", (float) cooling_info->flowrate[f].value);
    }
    for (uint p = 0; p < cooling_info->num_power; p++) {
        dprintf(fd_cooling, ",");
        dprintf(fd_cooling, "%f", (float) cooling_info->power[p].value);
    }
    dprintf(fd_cooling, ",%lu", cooling_info->total_power);

    for (uint s = 0; s < cooling_info->num_ads_chiller_loops; s++) {
        dprintf(fd_cooling, ",%f,%f,%f,%f,%f,%f,%f,%f,%f,%f", (float) cooling_info->in_temp_mt[s].value,
                (float) cooling_info->out_temp_mt[s].value, (float) cooling_info->in_temp_lt[s].value,
                (float) cooling_info->out_temp_lt[s].value, (float) cooling_info->evaporator_temp[s].value,
                (float) cooling_info->condenser_temp[s].value, (float) cooling_info->chiller_power[s].value,
                (float) cooling_info->pump_power[s].value, (float) cooling_info->flowrate_mt[s].value,
                (float) cooling_info->flowrate_lt[s].value);
    }
    for (uint s = 0; s < cooling_info->num_flowrate; s++) {
        dprintf(fd_cooling, ",%f", cooling_info->max_dev_temp[s]);
    }
    dprintf(fd_cooling, "\n");

    return EAR_SUCCESS;
}

state_t report_dispose(report_id_t *id)
{
    if (!must_report)
        return EAR_SUCCESS;
    if (fd_cooling >= 0) {
        close(fd_cooling);
        fd_cooling = -1;
    }
    return EAR_SUCCESS;
}
