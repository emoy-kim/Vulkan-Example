cmake_minimum_required(VERSION 3.8)

project(vulkan_framework)

include(cmake/check-compiler.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -D_RELEASE")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

set(
	SOURCE_FILES
        main.cpp
        source/renderer.cpp
)

include_directories("include")
include(cmake/add-libraries-linux.cmake)

#if(USE_HEADLESS)
#	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_HEADLESS_EXT")
#else()
   #find_package(XCB REQUIRED)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")
#endif()

add_executable(vulkan_framework ${SOURCE_FILES})

include(cmake/target-link-libraries-linux.cmake)

target_include_directories(vulkan_framework PUBLIC ${CMAKE_BINARY_DIR})


#==============================================================================
# COMPILE SHADERS
#==============================================================================
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders)
file(GLOB SHADERS
  ${SHADER_SOURCE_DIR}/*.vert
  ${SHADER_SOURCE_DIR}/*.frag
  ${SHADER_SOURCE_DIR}/*.comp
  ${SHADER_SOURCE_DIR}/*.geom
)

add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_SOURCE_DIR}
  OUTPUT ${SHADER_SOURCE_DIR}
)

foreach(source IN LISTS SHADERS)
  get_filename_component(FILENAME ${source} NAME)
  add_custom_command(
    COMMAND
      ${glslc_executable} -o ${SHADER_SOURCE_DIR}/${FILENAME}.spv ${source}
    OUTPUT ${SHADER_SOURCE_DIR}/${FILENAME}.spv
    DEPENDS ${source} ${SHADER_SOURCE_DIR}
    COMMENT "Compiling ${FILENAME}"
  )
  list(APPEND SPV_SHADERS ${SHADER_SOURCE_DIR}/${FILENAME}.spv)
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPV_SHADERS})