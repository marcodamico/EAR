############################################################################
# Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
#
# This program and the accompanying materials are made
# available under the terms of the Eclipse Public License 2.0
# which is available at https://www.eclipse.org/legal/epl-2.0/
#
# SPDX-License-Identifier: EPL-2.0
############################################################################
##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    EAR Team
#
#  SYNOPSIS:
#    X_AC_MODBUS()
#
#  DESCRIPTION:
#    Check the usual suspects for a modbus installation,
#    updating CFLAGS and LDFLAGS as necessary.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC and before AC_PROG_LIBTOOL.
##*****************************************************************************


AC_DEFUN([X_AC_MODBUS_FIND_ROOT_DIR],
[
    for d in $_x_ac_modbus_dirs_root;
    do
        test -d "$d" || continue
        test -f "$d/include/modbus/modbus.h" || continue

        AS_VAR_SET(_cv_modbus_dir_root, $d)
        test -n "$_cv_modbus_dir_root" && break
    done
])

AC_DEFUN([X_AC_MODBUS],
[
    _x_ac_modbus_dirs_root="/usr /usr/local /opt/modbus"
    _x_ac_modbus_dirs_incl="include include64"

    AC_ARG_WITH(
        [modbus],
        AS_HELP_STRING(--with-modbus=PATH,Specify path to libmodbus installation),
        [
            _x_ac_modbus_dirs_root="$withval"
            _x_ac_modbus_custom="yes"
        ]
    )

    AC_CACHE_CHECK(
        [for MODBUS root directory],
        [_cv_modbus_dir_root],
        [
            X_AC_MODBUS_FIND_ROOT_DIR([])

            if test -z "$_cv_modbus_dir_root"; then
                _x_ac_modbus_dirs_root="${_ax_ld_dirs_root}"
                _x_ac_modbus_custom="yes"

                X_AC_MODBUS_FIND_ROOT_DIR([])
            fi
        ]
    )

    if test -z "$_cv_modbus_dir_root"; then
        echo checking for MODBUS CFLAGS... no
    else
        MODBUS_DIR=$_cv_modbus_dir_root
        MODBUS_CFLAGS="-I\$(MODBUS_BASE)/include/modbus -DMODBUS_BASE=\\\"\$(MODBUS_BASE)\\\""
        MODBUS_LDFLAGS="-L\$(MODBUS_BASE)/lib -lmodbus"
        echo checking for MODBUS CFLAGS... $MODBUS_CFLAGS
        echo checking for MODBUS LDFLAGS... $MODBUS_LDFLAGS
    fi

    AC_SUBST(MODBUS_DIR)
    AC_SUBST(MODBUS_CFLAGS)
    AC_SUBST(MODBUS_LDFLAGS)

    AM_CONDITIONAL(WITH_MODBUS, test -n "$_cv_modbus_dir_root")
])