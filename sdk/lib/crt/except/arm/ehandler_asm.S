/*
 * PROJECT:     LiberNT C Runtime
 * LICENSE:     LGPL-2.1-or-later (https://spdx.org/licenses/LGPL-2.1-or-later)
 * PURPOSE:     Invoke ARM32 filters with the protected frame's nonvolatile registers
 * COPYRIGHT:   Copyright 2002 Marcus Meissner, SuSE Linux AG
 *              Copyright 2010-2013, 2015 André Hentschel
 */

/*
 * ARM signal handling routines
 *
 * Copyright 2002 Marcus Meissner, SuSE Linux AG
 * Copyright 2010-2013, 2015 André Hentschel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <ksarm.h>

    TEXTAREA

    NESTED_ENTRY __C_ExecuteExceptionFilter
    push.w {r3-r11, lr}
    .seh_save_regs_w {r3-r11, lr}
    vpush {d8-d15}
    .seh_save_fregs {d8-d15}
    PROLOG_END
    add r12, r3, #32
    vldmia r12, {d8-d15}
    ldmia r3, {r4-r11}
    blx r2
    .seh_startepilogue
    vpop {d8-d15}
    .seh_save_fregs {d8-d15}
    pop.w {r3-r11, pc}
    .seh_save_regs_w {r3-r11, lr}
    .seh_endepilogue
    NESTED_END

    END
