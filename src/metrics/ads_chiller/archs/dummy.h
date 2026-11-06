/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef ADS_CHILLER_DUMMY_H
#define ADS_CHILLER_DUMMY_H

#include <metrics/ads_chiller/ads_chiller.h>

void ads_chiller_dummy_load(ads_chiller_ops_t *ops);

void ads_chiller_dummy_get_info(apinfo_t *info);

void ads_chiller_dummy_get_devices(ads_chiller_devs_t **devs, uint *devs_count);

state_t ads_chiller_dummy_init(ctx_t *c);

state_t ads_chiller_dummy_dispose(ctx_t *c);

state_t ads_chiller_dummy_read(ctx_t *c, ads_chiller_t *data);

state_t ads_chiller_dummy_read_raw(ctx_t *c, ads_chiller_t *data);

float ads_chiller_dummy_get_metric_mult(void);

int ads_chiller_dummy_get_is_kelvin(void);

#endif // ADS_CHILLER_DUMMY_H