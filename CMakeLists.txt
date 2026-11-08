cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME F407-TargetCar)


# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Bsp/CAN/Src/bsp_can.cpp
        Bsp/CAN/Inc/bsp_can.h
        Bsp/USART/Src/esp01s.cpp
        Bsp/USART/Inc/esp01s.h
        Device/CAN/can_dispatcher.cpp
        Device/CAN/can_dispatcher.h
        Bsp/CAN/can_config.h
        Device/CAN/Motor/motor_manager.cpp
        Device/CAN/Motor/motor_manager.h
        Device/CAN/Motor/motor_common.h
        Bsp/CAN/can_byte_utils.h
        Device/CAN/can_app.cpp
        Device/CAN/can_app.h
        Device/CAN/can_device_config.cpp
        Device/CAN/can_device_config.h
        Device/CAN/Motor/Src/dji_motor.cpp
        Device/CAN/Motor/Inc/dji_motor.h
        Task/Inc/Rotor.h
        Task/Src/Rotor.cpp
        Task/Inc/EspLink.h
        Task/Src/EspLink.cpp
        PID/Inc/speed_pid.h
        PID/Src/speed_pid.cpp
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
        # Bsp（板级支持包）层
        Bsp/CAN
        Bsp/CAN/Inc
        Bsp/USART/Inc
        Device/CAN/
        Device/CAN/Motor
        Device/CAN/Motor/Inc
        Task/Inc
        Task
        PID/Inc
        PID
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
