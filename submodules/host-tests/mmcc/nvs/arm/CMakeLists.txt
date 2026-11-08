# PROJECT:     LiberNT API tests
# LICENSE:     GPL-3.0-or-later
# PURPOSE:     Native ARM LPAE descriptor checks
# COPYRIGHT:   Copyright 2026 LiberNT contributors

foreach(TARGET mm_arm_pte mm_arm_pte_asan)
    add_executable(${TARGET} t_pte.c
        ${MMCC_KERNEL_ROOT}/nvs/arch/arm/pte.c
        ${MMCC_KERNEL_ROOT}/nvs/arch/arm/descriptor.c)
    target_compile_definitions(${TARGET} PRIVATE MM_HOST_TEST)
    target_include_directories(${TARGET} PRIVATE ${MMCC_KERNEL_ROOT})
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Werror -g -O1)
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endforeach()
target_compile_options(mm_arm_pte_asan PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(mm_arm_pte_asan PRIVATE -fsanitize=address,undefined)
