/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef TWO_PHASE_MODBUS_H
#define TWO_PHASE_MODBUS_H

#include <metrics/common/modbus/two_phase_modbus.h>
#include <metrics/two_phase/two_phase.h>

void two_phase_modbus_load(two_phase_ops_t *ops, int force_api);

state_t two_phase_modbus_init(ctx_t *c);

state_t two_phase_modbus_pool(void *p);

state_t two_phase_modbus_read(ctx_t *c, two_phase_t *data);

#endif // TWO_PHASE_MODBUS_H