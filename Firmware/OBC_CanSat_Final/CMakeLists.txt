cmake_minimum_required(VERSION 3.22)

# Set the project name and enable languages
project(CanSat_Final C ASM)

# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms 
    USE_HAL_DRIVER 
    STM32F411xE
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths
set(MX_Include_Dirs
    Core/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/crc.c
    Core/Src/i2c.c
    Core/Src/spi.c
    Core/Src/usart.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
    Core/Src/sysmem.c
    Core/Src/syscalls.c
    Core/Src/euler.c
    Core/Src/sh2_SensorValue.c
    Core/Src/sh2_util.c
    Core/Src/sh2.c
    Core/Src/shtp.c
    Core/Src/sx1278.c
    Core/Src/telemetry.c
    startup_stm32f411xe.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    Core/Src/system_stm32f4xx.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_crc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
)

# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create the executable
add_executable(${CMAKE_PROJECT_NAME})

# Create STM32_Drivers static library
add_library(STM32_Drivers STATIC ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)

# Add application sources to the executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link libraries to the executable
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
    STM32_Drivers
    m
)

# Opciones de compilación para habilitar la depuración completa y desactivar optimizaciones (solo para Debug)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:-g3 -O0>)

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(FATAL_ERROR "Generated code requires C11 or higher")
endif()
