include_directories(${REACTOS_SOURCE_DIR}/hal/arch/common/include)
if(SARCH STREQUAL "virt")
    spec2def(hal.dll virt/hal.spec ADD_IMPORTLIB)
    add_definitions(-D_NTHALDLL_ -D_NTHAL_)
    include_directories(include ${REACTOS_SOURCE_DIR}/ntoskrnl/include
        ${REACTOS_SOURCE_DIR}/hal/arch/common/include)
    foreach(HAL_TARGET hal halup)
        add_library(${HAL_TARGET} MODULE
            virt/hal.c virt/spinlock.c virt/pnp.c virt/dma.c generic/profil.c
            ${REACTOS_SOURCE_DIR}/hal/arch/common/generic/dma_coherent.c
            exynos4210/cache.c
            ${REACTOS_SOURCE_DIR}/hal/arch/common/acpi/halpnpdd.c
            ${REACTOS_SOURCE_DIR}/hal/arch/common/acpi/tables.c
            ${CMAKE_CURRENT_BINARY_DIR}/hal.def)
        set_module_type(${HAL_TARGET} kerneldll ENTRYPOINT 0)
        target_link_libraries(${HAL_TARGET} halcommon libcntpr)
        add_importlibs(${HAL_TARGET} ntoskrnl)
        add_dependencies(${HAL_TARGET} xdk asm)
        add_cd_file(TARGET ${HAL_TARGET} DESTINATION reactos/system32 NO_CAB FOR all)
    endforeach()
    return()
endif()

spec2def(hal.dll ../hal.spec ADD_IMPORTLIB)

add_definitions(
    -D_NTHALDLL_
    -D_NTHAL_)

include_directories(
    include
    ${REACTOS_SOURCE_DIR}/ntoskrnl/include)

list(APPEND SOURCES
    omap3/halinit_up.c
    omap3/halup.rc)

add_library(hal MODULE
    ${SOURCES})

set_module_type(hal kerneldll ENTRYPOINT 0)

add_library(halup MODULE
    ${SOURCES})

set_module_type(halup kerneldll ENTRYPOINT 0)
add_cd_file(TARGET halup DESTINATION reactos/system32 NO_CAB FOR all)
