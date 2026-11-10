/*
 * PROJECT:     LiberNT API tests
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     ARM memory primitive alignment, overlap and boundary regression
 * COPYRIGHT:   Copyright 2026 LiberNT contributors
 */
#include <ntddk.h>
#include <string.h>

static void *(__cdecl *volatile Copy)(void *, const void *, size_t) = memcpy;
static void *(__cdecl *volatile Move)(void *, const void *, size_t) = memmove;
static void *(__cdecl *volatile Fill)(void *, int, size_t) = memset;

BOOLEAN ArmTestMemory(VOID)
{
    DECLSPEC_ALIGN(8) UCHAR Source[256], Buffer[256];
    ULONG Length, Src, Dst, Index;
    LONG Delta;
    UCHAR Expected;
    BOOLEAN Passed = TRUE;

    for (Index = 0; Index < sizeof(Source); ++Index) Source[Index] = (UCHAR)(Index * 37 + 11);
    for (Length = 0; Length <= 128; ++Length)
    {
        for (Src = 0; Src < 8; ++Src)
        {
            for (Dst = 0; Dst < 8; ++Dst)
            {
                Fill(Buffer, 0xcd, sizeof(Buffer));
                if (Copy(Buffer + 16 + Dst, Source + Src, Length) != Buffer + 16 + Dst) Passed = FALSE;
                for (Index = 0; Index < sizeof(Buffer); ++Index)
                {
                    Expected = Index >= 16 + Dst && Index < 16 + Dst + Length ?
                               Source[Src + Index - 16 - Dst] : 0xcd;
                    if (Buffer[Index] != Expected) Passed = FALSE;
                }
            }
            for (Delta = -8; Delta <= 8; ++Delta)
            {
                for (Index = 0; Index < sizeof(Buffer); ++Index) Buffer[Index] = Source[Index];
                Dst = (ULONG)(32 + Src + Delta);
                if (Move(Buffer + Dst, Buffer + 32 + Src, Length) != Buffer + Dst) Passed = FALSE;
                for (Index = 0; Index < sizeof(Buffer); ++Index)
                {
                    Expected = Index >= Dst && Index < Dst + Length ?
                               Source[32 + Src + Index - Dst] : Source[Index];
                    if (Buffer[Index] != Expected) Passed = FALSE;
                }
            }
            Fill(Buffer, 0xcd, sizeof(Buffer));
            if (Fill(Buffer + 16 + Src, 0x1ba, Length) != Buffer + 16 + Src) Passed = FALSE;
            for (Index = 0; Index < sizeof(Buffer); ++Index)
            {
                Expected = Index >= 16 + Src && Index < 16 + Src + Length ? 0xba : 0xcd;
                if (Buffer[Index] != Expected) Passed = FALSE;
            }
        }
    }
    return Passed;
}
