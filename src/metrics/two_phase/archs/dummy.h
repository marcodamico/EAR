/*********************************************************************
 * Copyright (c) 2025 Energy Aware Solutions, S.L
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **********************************************************************/

#ifndef TWO_PHASE_DUMMY_H
#define TWO_PHASE_DUMMY_H

#include <metrics/two_phase/two_phase.h>

void two_phase_dummy_load(two_phase_ops_t *ops);

void two_phase_dummy_get_info(apinfo_t *info);

void two_phase_dummy_get_devices(two_phase_devs_t **devs, uint *devs_count);

state_t two_phase_dummy_init(ctx_t *c);

state_t two_phase_dummy_dispose(ctx_t *c);

state_t two_phase_dummy_read(ctx_t *c, two_phase_t *data);

state_t two_phase_dummy_read_raw(ctx_t *c, two_phase_t *data);

float two_phase_dummy_get_metric_mult(void);

int two_phase_dummy_get_is_kelvin(void);

#endif // TWO_PHASE_DUMMY_H