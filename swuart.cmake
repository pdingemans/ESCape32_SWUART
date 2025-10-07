# Generic Software UART Library for ESCape32
# This library can be included in any MCU-specific build

cmake_minimum_required(VERSION 3.15)

# Function to add swuart library to a target
function(add_swuart_to_target target_name mcu_family)
    set(SWUART_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mcu/${mcu_family}/swuart)
    
    # Check if MCU-specific swuart directory exists
    if(EXISTS ${SWUART_BASE_DIR})
        # Include swuart sources if they exist
        if(EXISTS ${SWUART_BASE_DIR}/src)
            file(GLOB SWUART_SOURCES ${SWUART_BASE_DIR}/src/*.c)
        endif()
        
        # Add sources to target if found
        if(SWUART_SOURCES)
            target_sources(${target_name} PRIVATE ${SWUART_SOURCES})
        endif()
        
        # Add include directory
        if(EXISTS ${SWUART_BASE_DIR}/inc)
            target_include_directories(${target_name} PRIVATE ${SWUART_BASE_DIR}/inc)
        endif()
        
        # Define SWUART_ENABLED for conditional compilation
        target_compile_definitions(${target_name} PRIVATE SWUART_ENABLED=1)
        
        message(STATUS "Added SWUART library for ${mcu_family} to target ${target_name}")
    else()
        message(WARNING "SWUART directory not found for MCU family: ${mcu_family}")
    endif()
endfunction()

# Create a header-only interface library for direct use
add_library(swuart_interface INTERFACE)

# Try to find a default swuart header (could be in AT32F421 as reference)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mcu/AT32F421/swuart/inc)
    target_include_directories(swuart_interface INTERFACE 
        ${CMAKE_CURRENT_SOURCE_DIR}/mcu/AT32F421/swuart/inc
    )
endif()

target_compile_definitions(swuart_interface INTERFACE SWUART_ENABLED=1)
