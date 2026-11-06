/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef ADS_CHILLER_MODBUS_H
#define ADS_CHILLER_MODBUS_H

#include <metrics/ads_chiller/ads_chiller.h>
#include <metrics/common/modbus/ads_chiller_modbus.h>

void ads_chiller_modbus_load(ads_chiller_ops_t *ops, int force_api);

state_t ads_chiller_modbus_init(ctx_t *c);

state_t ads_chiller_modbus_pool(void *p);

state_t ads_chiller_modbus_read(ctx_t *c, ads_chiller_t *data);

#endif // ADS_CHILLER_MODBUS_H