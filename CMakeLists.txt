cmake_minimum_required(VERSION 3.1)
project(sdbg)

set(CMAKE_MODULE_PATH "./cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)

add_subdirectory(./libs/ShaderDebugger)

# source code
set(SOURCES
	main.cpp
)

# cmake toolchain 
if(CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_TOOLCHAIN_FILE})
endif(CMAKE_TOOLCHAIN_FILE)

# glm
find_package(GLM REQUIRED)

# create executable
add_executable(sdbg ${SOURCES})

# properties
set_target_properties(sdbg PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# include directories
target_include_directories(sdbg PRIVATE ${GLM_INCLUDE_DIRS})
target_include_directories(sdbg PRIVATE libs)

# link libraries
target_link_libraries(sdbg ShaderDebugger)
target_link_libraries(sdbg ${GLM_LIBRARY_DIRS})

if (NOT MSVC)
	target_compile_options(sdbg PRIVATE -Wno-narrowing)
endif()

set(BINARY_INST_DESTINATION "bin")
install(PROGRAMS bin/sdbg DESTINATION "${BINARY_INST_DESTINATION}")
