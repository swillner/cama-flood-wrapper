cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
include(cmake/helpers.cmake)
include(cmake/libraries.cmake)

set_default_build_type(Release)

enable_language(Fortran)
project(camaflood)

add_library(libcamaflood
  cama-flood/lib/endian4.F
  cama-flood/lib/igetday.F
  cama-flood/lib/len_trim.F
  cama-flood/lib/lib_dates.F
  cama-flood/lib/lib_netcdf_util.F
  cama-flood/mod/mod_diag.F
  cama-flood/mod/mod_input.F
  cama-flood/mod/mod_map.F
  cama-flood/mod/mod_output.F
  cama-flood/mod/mod_prog.F
  cama-flood/mod/mod_time.F
  cama-flood/mod/parkind1.F
  cama-flood/src/calc_damout.F
  cama-flood/src/calc_fldout.F
  cama-flood/src/calc_fldstg.F
  cama-flood/src/calc_outpre.F
  cama-flood/src/calc_pthout.F
  cama-flood/src/calc_rivout.F
  cama-flood/src/calc_rivout_kine.F
  cama-flood/src/calc_stonxt.F
  cama-flood/src/calc_watbal.F
  cama-flood/src/control_inp.F
  cama-flood/src/control_out.F
  cama-flood/src/control_rest.F
  cama-flood/src/control_tstp.F
  cama-flood/src/create_outbin.F
  cama-flood/src/create_outcdf.F
  cama-flood/src/init_cond.F
  cama-flood/src/init_inputnam.F
  cama-flood/src/init_map.F
  cama-flood/src/init_time.F
  cama-flood/src/init_topo.F
  src/additional.f90
  )

find_package(Patch REQUIRED)
message(STATUS "Patch found: ${Patch_EXECUTABLE}")

function(patch_file FILENAME PATCH)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME})
  execute_process(
    COMMAND ${Patch_EXECUTABLE} ${FILENAME} ${PATCH} -o ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE OUTPUT_PATCH
    RESULT_VARIABLE RESULT_PATCH
    )
  string(STRIP "${OUTPUT_PATCH}" OUTPUT_PATCH)
  message(STATUS ${OUTPUT_PATCH})
  if(NOT "${RESULT_PATCH}" EQUAL "0")
    message(FATAL_ERROR "Patching failed: ${RESULT_PATCH}")
  endif()
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${FILENAME} PROPERTIES GENERATED TRUE)
  target_sources(libcamaflood PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME})
endfunction()

patch_file(cama-flood/src/control_phy.F patches/control_phy.patch)

add_executable(camaflood
  src/main.cpp
  )

target_link_libraries(camaflood PRIVATE libcamaflood)

#target_include_directories(cama_flood PRIVATE include lib/lmoments/include lib/cpp-library)
target_compile_options(camaflood PRIVATE -std=c++11)
target_compile_options(libcamaflood PRIVATE -cpp)
if(CMAKE_Fortran_COMPILER_ID STREQUAL Intel)
  target_compile_options(libcamaflood PRIVATE -convert little_endian -assume byterecl -heap-arrays)
endif()
set_target_properties(libcamaflood PROPERTIES Fortran_FORMAT FREE)

set_advanced_cpp_warnings(camaflood)
set_build_type_specifics(camaflood)
set_build_type_specifics(libcamaflood)
#add_git_version(camaflood
#  FALLBACK_VERSION 1.0.0)

option(WITH_NETCDF "" ON)
if(WITH_NETCDF)
  find_package(NETCDF_Fortran REQUIRED)
  message(STATUS "NetCDF Fortran include directory: ${NETCDF_FORTRAN_INCLUDE_DIR}")
  message(STATUS "NetCDF Fortran library: ${NETCDF_FORTRAN_LIBRARY}")
  include_directories(${NETCDF_FORTRAN_INCLUDE_DIR})
  target_link_libraries(libcamaflood PRIVATE netcdff)
  target_compile_definitions(libcamaflood PRIVATE UseCDF)
endif()

option(WITH_ENDIAN_CONVERSION "" OFF)
if(WITH_ENDIAN_CONVERSION)
  target_compile_definitions(libcamaflood PRIVATE ConvEnd)
endif()

# TODO UseMPI

option(PARALLELIZATION "" ON)
if(PARALLELIZATION)
  find_package(OpenMP REQUIRED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} ${OpenMP_Fortran_FLAGS}")
endif()

#include(lib/settingsnode/settingsnode.cmake)
#include_settingsnode(cama_flood)

add_cpp_tools(camaflood)
