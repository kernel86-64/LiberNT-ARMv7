/*
 * PROJECT:     LiberNT HAL
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     QEMU virt bootstrap device addresses
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once

#define ARM_VIRT_UART_PHYSICAL 0x09000000UL
#define ARM_VIRT_UART_VIRTUAL  0xFFF00000UL
#define ARM_VIRT_GIC_DIST      0x08000000UL
#define ARM_VIRT_GIC_CPU       0x08010000UL
#define ARM_VIRT_RTC_PHYSICAL  0x09010000UL
#define ARM_VIRT_GIC_DIST_VIRTUAL 0xFFF10000UL
#define ARM_VIRT_GIC_CPU_VIRTUAL  0xFFF20000UL
#define ARM_VIRT_RTC_VIRTUAL      0xFFF30000UL

#define ARM_VIRT_TIMER_VECTOR 27UL
