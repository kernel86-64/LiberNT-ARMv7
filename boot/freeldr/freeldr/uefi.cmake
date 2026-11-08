##
## PROJECT:     FreeLoader
## LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
## PURPOSE:     Build definitions for UEFI
## COPYRIGHT:   Copyright 2023 Justin Miller <justinmiller100@gmail.com>
##

include_directories(BEFORE
    ${REACTOS_SOURCE_DIR}/sdk/include/reactos/edk2
    ${REACTOS_SOURCE_DIR}/boot/freeldr/freeldr
    ${REACTOS_SOURCE_DIR}/boot/freeldr/freeldr/include
    ${REACTOS_SOURCE_DIR}/boot/freeldr/freeldr/include/arch/uefi
    ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND UEFILDR_ARC_SOURCE
    ${FREELDR_ARC_SOURCE}
    arch/uefi/ftglue.c
    arch/uefi/stubs.c
    arch/uefi/ueficon.c
    arch/uefi/uefidisk.c
    arch/uefi/uefihw.c
    arch/uefi/uefiinput.c
    arch/uefi/uefilog.c
    arch/uefi/uefimem.c
    arch/uefi/uefireboot.c
    arch/uefi/uefisetup.c
    arch/uefi/uefiutil.c
    arch/uefi/uefivid.c
    arch/uefi/uefisym.c
    arch/uefi/uefibacktrace.c
    arch/vidfb.c
    arch/vgafont.c)

if(FREELDR_HTTP_BOOT)
    list(APPEND UEFILDR_ARC_SOURCE
        arch/uefi/uefinet.c
        arch/uefi/uefinet_hw.c
        arch/uefi/uefidhcp.c
        arch/uefi/uefihttp.c)
endif()

if(ARCH STREQUAL "i386")
    list(APPEND UEFILDR_ARC_SOURCE
        arch/i386/i386idt.c)
    list(APPEND UEFILDR_COMMON_ASM_SOURCE
        arch/uefi/i386/uefiasm.S
        arch/i386/i386trap.S)
elseif(ARCH STREQUAL "amd64")
    list(APPEND UEFILDR_COMMON_ASM_SOURCE
        arch/uefi/amd64/uefiasm.S)
elseif(ARCH STREQUAL "arm")
    list(APPEND UEFILDR_ARC_SOURCE
        arch/arm/debug.c
        arch/uefi/uefiserial.c
        arch/uefi/arm/runtime.c)
    list(APPEND UEFILDR_COMMON_ASM_SOURCE
        arch/uefi/arm/uefiasm.S
        arch/uefi/arm/paging.S
        arch/uefi/arm/entry.S)
elseif(ARCH STREQUAL "arm64")
    list(APPEND UEFILDR_ARC_SOURCE
        arch/uefi/arm64/early_uart.c
        arch/uefi/uefiserial.c)
    list(APPEND UEFILDR_COMMON_ASM_SOURCE
        arch/uefi/arm64/uefiasm.S)
elseif(ARCH STREQUAL "riscv64")
    list(APPEND UEFILDR_ARC_SOURCE
        arch/uefi/uefiserial.c
        arch/uefi/riscv64/runtime.c
        arch/uefi/riscv64/watchdog.c)
    list(APPEND UEFILDR_COMMON_ASM_SOURCE
        arch/uefi/riscv64/uefiasm.S)
else()
    #TBD
endif()

list(APPEND UEFILDR_BOOTMGR_SOURCE
    ${FREELDR_BOOTMGR_SOURCE}
    settingsmenu.c
    oslist.c
    ui/bootui.c
)

foreach(asset font icons logo)
    if(asset STREQUAL "font")
        set(asset_source ${REACTOS_SOURCE_DIR}/media/fonts/selawk.ttf)
    elseif(asset STREQUAL "icons")
        set(asset_source ${CMAKE_CURRENT_SOURCE_DIR}/ui/assets/icons.alpha)
    else()
        set(asset_source ${CMAKE_CURRENT_SOURCE_DIR}/ui/assets/logo.rgba)
    endif()
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bootui_${asset}.c ${CMAKE_CURRENT_BINARY_DIR}/bootui_${asset}.h
        COMMAND native-bin2c ${asset_source}
                ${CMAKE_CURRENT_BINARY_DIR}/bootui_${asset}.c
                ${CMAKE_CURRENT_BINARY_DIR}/bootui_${asset}.h BIN bootui_${asset}
        DEPENDS native-bin2c ${asset_source})
    list(APPEND UEFILDR_BOOTMGR_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/bootui_${asset}.c)
endforeach()

add_asm_files(uefifreeldr_common_asm ${FREELDR_COMMON_ASM_SOURCE} ${UEFILDR_COMMON_ASM_SOURCE})

list(APPEND FREELDR_NTLDR_SOURCE
    ${REACTOS_SOURCE_DIR}/ntoskrnl/config/cmboot.c
    ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/config.c
    ntldr/startup.c
    ntldr/conversion.c
    ntldr/headless.c
    ntldr/inffile.c
    ntldr/ntldropts.c
    ntldr/registry.c
    ntldr/setupldr.c
    ntldr/winldr.c
    ntldr/wlmemory.c
    ntldr/wlregistry.c)

if(ARCH STREQUAL "i386")
    list(APPEND FREELDR_NTLDR_SOURCE
        ntldr/arch/i386/winldr.c)
elseif(ARCH STREQUAL "amd64")
    list(APPEND FREELDR_NTLDR_SOURCE
        ntldr/arch/amd64/winldr.c)
elseif(ARCH STREQUAL "arm")
    list(APPEND FREELDR_NTLDR_SOURCE
        ntldr/arch/arm/winldr.c)
elseif(ARCH STREQUAL "arm64")
    list(APPEND FREELDR_NTLDR_SOURCE
        ntldr/arch/arm64/winldr.c)
elseif(ARCH STREQUAL "riscv64")
    list(APPEND FREELDR_NTLDR_SOURCE
        ntldr/arch/riscv64/jh7110.c
        ntldr/arch/riscv64/winldr.c)
else()
    #TBD
endif()

add_library(uefifreeldr_common
    ${uefifreeldr_common_asm}
    ${UEFILDR_ARC_SOURCE}
    ${FREELDR_BOOTLIB_SOURCE}
    ${UEFILDR_BOOTMGR_SOURCE}
    ${FREELDR_NTLDR_SOURCE})

# Keep setjmp before bootfont so the archive does not satisfy FreeType's
# longjmp reference with the vcruntime implementation, which needs RtlUnwind.
target_link_libraries(uefifreeldr_common setjmp bootfont)
target_link_libraries(uefifreeldr_common fatfs)
if(ARCH STREQUAL "riscv64")
    target_include_directories(uefifreeldr_common PRIVATE ${REACTOS_SOURCE_DIR}/sdk/lib/drivers/jh7110disp)
    target_link_libraries(uefifreeldr_common jh7110disp)
endif()

target_compile_definitions(uefifreeldr_common PRIVATE _FRLDRLIB_ UEFIBOOT)
if(FREELDR_HTTP_BOOT)
    target_compile_definitions(uefifreeldr_common PRIVATE FREELDR_HTTP_BOOT=1)
endif()
if(FREELDR_WIM_RAMDISK)
    target_compile_definitions(uefifreeldr_common PRIVATE FREELDR_WIM_RAMDISK=1)
endif()

if((CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang") AND
   (ARCH STREQUAL "i386" OR ARCH STREQUAL "amd64"))
    # Prevent using SSE (no support in freeldr)
    target_compile_options(uefifreeldr_common PUBLIC -mno-sse)
endif()

set(PCH_SOURCE
    ${UEFILDR_ARC_SOURCE}
    ${FREELDR_BOOTLIB_SOURCE}
    ${UEFILDR_BOOTMGR_SOURCE}
    ${FREELDR_NTLDR_SOURCE})

add_pch(uefifreeldr_common include/arch/uefi/uefildr.h PCH_SOURCE)
add_dependencies(uefifreeldr_common bugcodes asm xdk)

## GCC builds need this extra thing for some reason...
if(ARCH STREQUAL "i386" AND NOT MSVC)
    target_link_libraries(uefifreeldr_common mini_hal)
endif()


spec2def(uefildr.exe freeldr.spec)

list(APPEND UEFILDR_BASE_SOURCE
    include/arch/uefi/uefildr.h
    arch/uefi/uefildr.c
    bootmgr.c
    ${FREELDR_BASE_SOURCE})

if(ARCH STREQUAL "i386")
    # Must be included together with disk/scsiport.c
    list(APPEND UEFILDR_BASE_SOURCE
        ${CMAKE_CURRENT_BINARY_DIR}/uefildr.def)
endif()

add_executable(uefildr ${UEFILDR_BASE_SOURCE})
set_target_properties(uefildr PROPERTIES SUFFIX ".efi")

target_compile_definitions(uefildr PRIVATE _FRLDRLIB_ UEFIBOOT)

# On AMD64 we only map 1GB with freeloader, tell UEFI to keep us low.
# On ARM64, match the Windows boot manager preferred base to keep PE metadata
# and early VA/PA assumptions close to the platform loader this path emulates.
if(ARCH STREQUAL "amd64")
    set_image_base(uefildr 0x10000)
elseif(ARCH STREQUAL "arm64")
    set_image_base(uefildr 0x140000000)
    target_link_options(uefildr PRIVATE -Wl,--stack,0x200000)
endif()

if(MSVC)
if(NOT ARCH STREQUAL "arm")
    target_link_options(uefildr PRIVATE /DYNAMICBASE:NO)
endif()
    if(ARCH STREQUAL "i386" OR ARCH STREQUAL "amd64")
        target_link_options(uefildr PRIVATE /FILEALIGN:512 /ALIGN:512)
    endif()
    target_link_options(uefildr PRIVATE /NXCOMPAT:NO /ignore:4078 /ignore:4254 /DRIVER)
    # We don't need hotpatching
    remove_target_compile_option(uefildr "/hotpatch")
else()
    if(ARCH STREQUAL "arm" OR ARCH STREQUAL "arm64" OR ARCH STREQUAL "riscv64")
        target_link_options(uefildr PRIVATE -Wl,--exclude-all-symbols,--file-alignment,0x200,--section-alignment,0x1000)
    else()
        target_link_options(uefildr PRIVATE -Wl,--exclude-all-symbols,--file-alignment,0x200,--section-alignment,0x200)
    endif()
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_link_options(uefildr PRIVATE -Wl,/driver)
    endif()
    # Strip everything, including the compressed symbol section
    add_custom_command(TARGET uefildr
                    POST_BUILD
                    COMMAND ${CMAKE_STRIP} --remove-section=.symz $<TARGET_FILE:uefildr>
                    COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:uefildr>)
endif()

if(MSVC)
    set_subsystem(uefildr EFI_APPLICATION)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    if(ARCH STREQUAL "arm64")
        target_link_options(uefildr PRIVATE
            -Wl,--subsystem,efi_application:6.00
            -Wl,--major-os-version,6
            -Wl,--minor-os-version,0
            -Wl,--major-subsystem-version,6
            -Wl,--minor-subsystem-version,0)
    else()
        set_subsystem(uefildr efi_application)
    endif()
else()
    set_subsystem(uefildr 10)
endif()

if(ARCH STREQUAL "arm")
    set_entrypoint(uefildr ArmEfiEntry)
else()
    set_entrypoint(uefildr EfiEntry)
endif()

target_link_libraries(uefildr uefifreeldr_common cportlib blcmlib blrtl libcntpr)
if(FREELDR_WIM_RAMDISK)
    target_link_libraries(uefildr freeldr_wimcore)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_libraries(uefildr setjmp)
endif()
if(ARCH STREQUAL "i386")
    target_link_libraries(uefildr mini_hal)
endif()
if(ARCH STREQUAL "arm64")
    set_target_properties(uefildr PROPERTIES REACTOS_LSE_ATOMICS lse_atomics_boot)
endif()

# dynamic analysis switches
if(STACK_PROTECTOR)
    target_link_libraries(uefildr gcc_ssp_nt)
endif()

if(RUNTIME_CHECKS)
    target_link_libraries(uefildr runtmchk)
endif()

add_dependencies(uefildr xdk)

if(ARCH STREQUAL "arm" AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    find_program(ARM_UEFI_PYTHON NAMES python3 REQUIRED NO_CMAKE_FIND_ROOT_PATH)
    add_custom_command(TARGET uefildr POST_BUILD
        COMMAND ${ARM_UEFI_PYTHON} ${REACTOS_SOURCE_DIR}/sdk/tools/arm_uefi.py $<TARGET_FILE:uefildr>
        VERBATIM)
    set_property(TARGET uefildr APPEND PROPERTY LINK_DEPENDS ${REACTOS_SOURCE_DIR}/sdk/tools/arm_uefi.py)
endif()

if(ARCH STREQUAL "arm" AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_library(arm_abi_layout OBJECT ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/layout.c)
    add_dependencies(arm_abi_layout psdk)
    add_executable(arm_uefi_test
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/abi.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/reloc.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/debugsvc.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/syscall.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/syscall_impl.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/syscall.S
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/control.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/debugsvc.c
        ${REACTOS_SOURCE_DIR}/sdk/lib/rtl/arm/reloc.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/registers.S
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/unwind.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/control.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/kernel.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/traps.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/switch.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/ctxframe.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/process.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/ctxswtch.s
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/trap.s
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/trapframe.c
        ${REACTOS_SOURCE_DIR}/sdk/lib/rtl/arm/unwind.c
        ${REACTOS_SOURCE_DIR}/sdk/lib/rtl/arm/debug_asm.S
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ex/arm/ioport.s
        arch/uefi/arm/entry.S)
    set_source_files_properties(
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/syscall.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/syscall_impl.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/debugsvc.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/control.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/debugsvc.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/control.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/kernel.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/traps.c
        ${REACTOS_SOURCE_DIR}/modules/rostests/boot/arm/switch.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/ctxframe.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/process.c
        ${REACTOS_SOURCE_DIR}/ntoskrnl/ke/arm/trapframe.c PROPERTIES
        COMPILE_OPTIONS "-U_BLDR_;-U_NTHAL_"
        COMPILE_DEFINITIONS "_NTOSKRNL_;NTKRNLVISTA;NVS;EX_LEGACY_PUSH_LOCKS")
    target_include_directories(arm_uefi_test PRIVATE
        ${REACTOS_SOURCE_DIR}
        ${REACTOS_SOURCE_DIR}/sdk/lib/drivers/arblib
        ${REACTOS_BINARY_DIR}/ntoskrnl/include
        ${REACTOS_BINARY_DIR}/ntoskrnl/include/internal
        ${REACTOS_SOURCE_DIR}/sdk/include/reactos/drivers
        ${REACTOS_SOURCE_DIR}/ntoskrnl/nvs/arch/arm)
    set_target_properties(arm_uefi_test PROPERTIES SUFFIX ".efi")
    set_subsystem(arm_uefi_test efi_application)
    set_entrypoint(arm_uefi_test ArmEfiEntry)
    target_link_options(arm_uefi_test PRIVATE -Wl,/driver,--exclude-all-symbols,--section-alignment,0x1000)
    target_link_libraries(arm_uefi_test setjmp libcntpr)
    target_compile_definitions(arm_uefi_test PRIVATE UEFIBOOT)
    add_dependencies(arm_uefi_test xdk asm arm_abi_layout)
    add_custom_command(TARGET arm_uefi_test POST_BUILD
        COMMAND ${ARM_UEFI_PYTHON} ${REACTOS_SOURCE_DIR}/sdk/tools/arm_uefi.py $<TARGET_FILE:arm_uefi_test>
        VERBATIM)
    set_property(TARGET arm_uefi_test APPEND PROPERTY LINK_DEPENDS ${REACTOS_SOURCE_DIR}/sdk/tools/arm_uefi.py)
endif()
