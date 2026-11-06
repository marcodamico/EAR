/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef COMMON_TWO_PHASE_MODBUS_H
#define COMMON_TWO_PHASE_MODBUS_H

#define METRIC_MULT 100
#define TEMP_KELVIN 1

#include <common/system/monitor.h>
#include <metrics/two_phase/two_phase.h>

extern uint two_phase_ok_pool;
extern uint two_phase_modbus_devs_count;
extern two_phase_t *two_phase_pool_registers;
extern suscription_t *two_phase_sus;
extern uint two_phase_initialized;

void base_two_phase_modbus_load(two_phase_ops_t *ops, int force_api);

void two_phase_modbus_get_info(apinfo_t *info);

void two_phase_modbus_get_devices(two_phase_devs_t **devs, uint *two_phase_modbus_devs_count);

state_t base_two_phase_modbus_init(ctx_t *c);

state_t two_phase_modbus_dispose(ctx_t *c);

state_t two_phase_modbus_pool(void *c);

state_t two_phase_modbus_read(ctx_t *c, two_phase_t *data);

/* Reads the data directly from the Adsorption chiller API
 * (data is not ordered according to the two_phase_metric_type enum).
 */
state_t two_phase_modbus_read_raw(ctx_t *c, two_phase_t *data);

// state_t two_phase_modbus_write(ctx_t *c, int reg, uint16_t value);

void two_phase_modbus_get_value(two_phase_metric_type metric, int dev_idx, uint16_t *value);
void two_phase_modbus_get_value_all(two_phase_metric_type metric, uint16_t *values);
state_t two_phase_modbus_set_value(ctx_t *c, two_phase_metric_type metric, int dev_idx, double value);
state_t two_phase_modbus_set_value_all(ctx_t *c, two_phase_metric_type metric, const double *values);

void two_phase_reorder_metrics(two_phase_t *data, int i);
state_t two_phase_modbus_static_read(int i, two_phase_t *metric);
float two_phase_modbus_get_metric_mult(void);
int two_phase_modbus_get_is_kelvin(void);

#endif // COMMON_TWO_PHASE_MODBUS_H