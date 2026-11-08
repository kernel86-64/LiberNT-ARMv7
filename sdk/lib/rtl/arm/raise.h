/*
 * PROJECT:     LiberNT Run-Time Library
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM32 exception raising policy
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */

#pragma once

#define RTLP_RAISE_EXCEPTION_NEEDS_CALLER_CONTEXT
#define RTLP_EXECUTE_HANDLER_TAKES_ROUTINE
#define RTLP_RAISE_USES_RESTORE_CONTEXT
