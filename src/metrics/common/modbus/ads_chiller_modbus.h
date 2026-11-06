/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef COMMON_ADS_CHILLER_MODBUS_H
#define COMMON_ADS_CHILLER_MODBUS_H

#define METRIC_MULT 100
#define TEMP_KELVIN 1

#include <common/system/monitor.h>
#include <metrics/ads_chiller/ads_chiller.h>

extern uint ads_chiller_ok_pool;
extern uint ads_chiller_modbus_devs_count;
extern ads_chiller_t *ads_chiller_pool_registers;
extern suscription_t *ads_chiller_sus;
extern uint ads_chiller_initialized;

void base_ads_chiller_modbus_load(ads_chiller_ops_t *ops, int force_api);

void ads_chiller_modbus_get_info(apinfo_t *info);

void ads_chiller_modbus_get_devices(ads_chiller_devs_t **devs, uint *ads_chiller_modbus_devs_count);

state_t base_ads_chiller_modbus_init(ctx_t *c);

state_t ads_chiller_modbus_dispose(ctx_t *c);

state_t ads_chiller_modbus_read(ctx_t *c, ads_chiller_t *data);

/* Reads the data directly from the Adsorption chiller API
 * (data is not ordered according to the ads_chiller_metric_type enum).
 */
state_t ads_chiller_modbus_read_raw(ctx_t *c, ads_chiller_t *data);

// state_t ads_chiller_modbus_write(ctx_t *c, int reg, uint16_t value);

void ads_chiller_modbus_get_value(ctx_t *c, ads_chiller_metric_type metric, int dev_idx, uint16_t *value);
void ads_chiller_modbus_get_value_all(ctx_t *c, ads_chiller_metric_type metric, uint16_t *values);
state_t ads_chiller_modbus_set_value(ctx_t *c, ads_chiller_metric_type metric, int dev_idx, double value);
state_t ads_chiller_modbus_set_value_all(ctx_t *c, ads_chiller_metric_type metric, const double *values);

void ads_chiller_modbus_reorder_metrics(ads_chiller_t *data, int i);
state_t ads_chiller_modbus_static_read(int i, ads_chiller_t *metric);
float ads_chiller_modbus_get_metric_mult(void);
int ads_chiller_modbus_get_is_kelvin(void);
#endif // COMMON_ADS_CHILLER_MODBUS_H
