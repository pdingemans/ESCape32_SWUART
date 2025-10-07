# Software UART Library for ESCape32
# Compatible with libopencm3 and multiple MCU families

cmake_minimum_required(VERSION 3.15)

# Library name
set(SWUART_LIB_NAME swuart)

# Source files (add .c files here when implemented)
set(SWUART_SOURCES
    # src/singlewire_sw_uart.c  # Uncomment when implementation is added
)

# Header files
set(SWUART_HEADERS
    inc/singlewire_sw_uart.h
)

# Check if we have source files or if it's header-only
if(SWUART_SOURCES)
    # Create static library if we have source files
    add_library(${SWUART_LIB_NAME} STATIC ${SWUART_SOURCES} ${SWUART_HEADERS})
    
    # Set include directories for the library
    target_include_directories(${SWUART_LIB_NAME} PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:inc>
    )
    
    # Link against libopencm3 if available
    if(TARGET opencm3_stm32f1)
        target_link_libraries(${SWUART_LIB_NAME} PUBLIC opencm3_stm32f1)
    endif()
    
else()
    # Create interface library for header-only
    add_library(${SWUART_LIB_NAME} INTERFACE)
    
    # Set include directories for header-only library
    target_include_directories(${SWUART_LIB_NAME} INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:inc>
    )
endif()

# Compiler requirements
target_compile_features(${SWUART_LIB_NAME} INTERFACE c_std_99)

# Export the library for use by parent projects
set(SWUART_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/inc CACHE INTERNAL "SWUART include directory")
set(SWUART_LIBRARY ${SWUART_LIB_NAME} CACHE INTERNAL "SWUART library target")

# Optional: Install rules (if needed for system-wide installation)
if(SWUART_INSTALL)
    include(GNUInstallDirs)
    
    install(TARGETS ${SWUART_LIB_NAME}
        EXPORT swuart-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    install(FILES ${SWUART_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/swuart
    )
    
    install(EXPORT swuart-targets
        FILE swuart-config.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/swuart
    )
endif()
