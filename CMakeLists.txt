cmake_minimum_required(VERSION 3.1)
project(sdbg)

set(CMAKE_MODULE_PATH "./cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)

add_subdirectory(libs/SPIRV-VM)

# glslang
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Enables building of SPVRemapper" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Builds glslangValidator and spirv-remap" FORCE)
add_subdirectory(libs/glslang)

# source code
set(SOURCES
	main.cpp
)

# cmake toolchain 
if(CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_TOOLCHAIN_FILE})
endif(CMAKE_TOOLCHAIN_FILE)

# create executable
add_executable(sdbg ${SOURCES})

# properties
set_target_properties(sdbg PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# include directories
target_include_directories(sdbg PRIVATE libs libs/SPIRV-VM/inc libs/glslang)

# link libraries
target_link_libraries(sdbg glslang SPIRV SPIRVVM)

if (NOT MSVC)
	target_compile_options(sdbg PRIVATE -Wno-narrowing)
endif()

set(BINARY_INST_DESTINATION "bin")
install(PROGRAMS bin/sdbg DESTINATION "${BINARY_INST_DESTINATION}")
