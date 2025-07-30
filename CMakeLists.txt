cmake_minimum_required(VERSION 3.16)
PROJECT(ChimeraTK-ApplicationCore-MicroDAQ)

# ______________________________________________________________________________
# OPTIONS
option(ENABLE_ROOT "Add support for ROOT based DAQ" OFF)
option(ENABLE_HDF5 "Add support for HDF5 based DAQ" ON)
option(BUILD_TESTS "Build tests" ON)

# ______________________________________________________________________________
# VERSION
set(${PROJECT_NAME}_MAJOR_VERSION 03)
set(${PROJECT_NAME}_MINOR_VERSION 03)
set(${PROJECT_NAME}_PATCH_VERSION 01)
include(${CMAKE_SOURCE_DIR}/cmake/set_version_numbers.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/set_default_flags.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/set_default_build_to_release.cmake)

# ______________________________________________________________________________
# Dependencies
find_package(ChimeraTK-ApplicationCore 04.00 REQUIRED)

# extract ApplicationCore include dir from the chimeraTK target - it is needed to build the ROOT dictionary
get_target_property(ChimeraTK-ApplicationCore_INCLUDE_DIRS ChimeraTK::ChimeraTK-ApplicationCore INTERFACE_INCLUDE_DIRECTORIES)
include_directories(SYSTEM ${ChimeraTK-ApplicationCore_INCLUDE_DIRS})
find_package(Boost COMPONENTS date_time REQUIRED)

IF(ENABLE_HDF5)
  FIND_PACKAGE(HDF5 REQUIRED COMPONENTS C CXX HL)
ENDIF(ENABLE_HDF5)

IF(ENABLE_ROOT)
  FIND_PACKAGE(ROOT 6.08 REQUIRED COMPONENTS Core Tree Gui)
  INCLUDE(${ROOT_USE_FILE})

  # first this since it sets -std=c++11
  ADD_DEFINITIONS(${ROOT_CXX_FLAGS})
ENDIF(ENABLE_ROOT)

# now set latest c++ support
include(cmake/enable_latest_cxx_support.cmake)

# ______________________________________________________________________________
# Includes
include_directories(${PROJECT_SOURCE_DIR} include ${HDF5_INCLUDE_DIRS} test)
file(GLOB headers "${PROJECT_SOURCE_DIR}/include/*.h")
file(GLOB test_headers "${PROJECT_SOURCE_DIR}/test/*.h")

# ______________________________________________________________________________
# Dictionary
IF(ENABLE_ROOT)
  set(ROOTDICTDAQ ${PROJECT_NAME}Dict)

  if(NOT ${${PROJECT_NAME}_BUILDVERSION} STREQUAL "")
    set(ROOTDICTDAQ ${ROOTDICTDAQ}_${${PROJECT_NAME}_MAJOR_VERSION}_${${PROJECT_NAME}_MINOR_VERSION}${${PROJECT_NAME}_BUILDVERSION}_${${PROJECT_NAME}_PATCH_VERSION})
  endif(NOT ${${PROJECT_NAME}_BUILDVERSION} STREQUAL "")

  message(STATUS "Dict Name: ${ROOTDICTDAQ}")
  ROOT_GENERATE_DICTIONARY(${ROOTDICTDAQ} include/data_types.h
    LINKDEF include/LinkDef.h)
ENDIF(ENABLE_ROOT)

# ______________________________________________________________________________
# Build target
set(source_MicroDAQ src/MicroDAQ.cc)
set(daq_header include/MicroDAQ.h)

IF(ENABLE_HDF5)
  # Append MicroDAQ based on HDF5
  list(APPEND source_MicroDAQ src/MicroDAQHDF5.cc)
  string(APPEND daq_header ";include/MicroDAQHDF5.h")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_HDF5")
ENDIF(ENABLE_HDF5)

IF(ENABLE_ROOT)
  string(APPEND daq_header ";include/MicroDAQROOT.h;include/data_types.h")

  # Append MicroDAQ based on ROOT
  list(APPEND source_MicroDAQ src/MicroDAQROOT.cc ${ROOTDICTDAQ})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_ROOT")
ENDIF(ENABLE_ROOT)

# Build library that contains the MicroDAQ module to be used in ChimeraTK
add_library(${PROJECT_NAME} SHARED ${source_MicroDAQ} ${headers})
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${daq_header}")
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
  SOVERSION ${${PROJECT_NAME}_SOVERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

# exported includes are all under ${CMAKE_INSTALL_PREFIX}/include
target_include_directories(${PROJECT_NAME} INTERFACE "$<INSTALL_INTERFACE:include>")

IF(ENABLE_ROOT)
  target_link_libraries(${PROJECT_NAME}
    PUBLIC ChimeraTK::ChimeraTK-ApplicationCore
    PRIVATE ${HDF5_HL_LIBRARIES}
    ${HDF5_CXX_LIBRARIES}
    ROOT::Tree
    ${Boost_LIBRARIES}
  )
ELSE()
  target_link_libraries(${PROJECT_NAME}
    PUBLIC ChimeraTK::ChimeraTK-ApplicationCore
    PRIVATE ${HDF5_HL_LIBRARIES}
    ${HDF5_CXX_LIBRARIES}
    ${Boost_LIBRARIES}
  )
ENDIF()

find_package(Boost COMPONENTS unit_test_framework)

if(BUILD_TESTS)
  if(Boost_UNIT_TEST_FRAMEWORK_FOUND)
    enable_testing()
    include(cmake/enable_code_style_check.cmake)
    add_subdirectory(test)
  else()
    message(STATUS "Boost test framework not found. No boost based tests will be build.")
  endif(Boost_UNIT_TEST_FRAMEWORK_FOUND)
endif(BUILD_TESTS)

# Install the library and the executables
# this defines architecture-dependent ${CMAKE_INSTALL_LIBDIR}
include(GNUInstallDirs)

if(ENABLE_ROOT)
  INSTALL(FILES ${PROJECT_BINARY_DIR}/lib${ROOTDICTDAQ}_rdict.pcm
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE
    GROUP_READ
    WORLD_READ)
endif(ENABLE_ROOT)

INSTALL(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/ChimeraTK/ApplicationCore
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE)

set(${PROJECT_NAME}_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")
set(${PROJECT_NAME}_LIBRARIES "${ChimeraTK-ApplicationCore_LIBRARIES} ${HDF5_LIBRARIES}")
set(${PROJECT_NAME}_LIBRARY_DIRS "${CMAKE_INSTALL_PREFIX}/lib")
set(${PROJECT_NAME}_CXX_FLAGS "${ChimeraTK-ApplicationCore_CXX_FLAGS}")
set(${PROJECT_NAME}_LINK_FLAGS "${ChimeraTK-ApplicationCore_LINK_FLAGS}")

list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "Boost COMPONENTS system thread chrono filesystem date_time atomic")

if(ENABLE_ROOT)
  list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "ROOT COMPONENTS Core Tree Gui")
endif(ENABLE_ROOT)

# we support our cmake EXPORTS as imported targets
set(PROVIDES_EXPORTED_TARGETS 1)
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
