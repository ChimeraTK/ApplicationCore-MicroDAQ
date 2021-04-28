PROJECT(ChimeraTK-ApplicationCore-MicroDAQ)
cmake_minimum_required(VERSION 2.8.12)

#______________________________________________________________________________
#                                                                       OPTIONS

option(ENABLE_ROOT "Add support for ROOT based DAQ" OFF)
option(ENABLE_HDF5 "Add support for HDF5 based DAQ" ON)
option(ENABLE_TESTING "Build tests" ON)

#______________________________________________________________________________
#                                                                       VERSION
set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 01)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(${CMAKE_SOURCE_DIR}/cmake/set_version_numbers.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/set_default_flags.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/set_default_build_to_release.cmake)


#______________________________________________________________________________
#                                                                  Dependencies
include(cmake/add_dependency.cmake)
add_dependency(ChimeraTK-ApplicationCore 02.02 REQUIRED)

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

#______________________________________________________________________________
#                                                                      Includes
include_directories(${PROJECT_SOURCE_DIR} include ${HDF5_INCLUDE_DIRS})

#______________________________________________________________________________
#                                                                    Dictionary
IF(ENABLE_ROOT)
  set(ROOTDICTDAQ ${PROJECT_NAME}Dict)
  ROOT_GENERATE_DICTIONARY( ${ROOTDICTDAQ} include/data_types.h
                                        LINKDEF include/LinkDef.h)
ENDIF(ENABLE_ROOT)

#______________________________________________________________________________
#                                                                  Build target

set(source_MicroDAQ src/MicroDAQ.cc)
SET(daq_header include/MicroDAQ.h)

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
add_library( ${PROJECT_NAME} SHARED ${source_MicroDAQ})
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${daq_header}")
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION}
                                                 SOVERSION ${${PROJECT_NAME}_SOVERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

IF(ENABLE_ROOT)
    target_link_libraries(${PROJECT_NAME}
        PUBLIC ${ChimeraTK-ApplicationCore_LIBRARIES}
        PRIVATE ${HDF5_HL_LIBRARIES}
                ${HDF5_CXX_LIBRARIES}
                ROOT::Tree
    )
ELSE()
    target_link_libraries(${PROJECT_NAME}
        PUBLIC ${ChimeraTK-ApplicationCore_LIBRARIES}
        PRIVATE ${HDF5_HL_LIBRARIES}
                ${HDF5_CXX_LIBRARIES}
    )
ENDIF()
  

find_package(Boost COMPONENTS unit_test_framework)                            
if(Boost_UNIT_TEST_FRAMEWORK_FOUND AND ENABLE_TESTING)
  enable_testing()
  
  if(ENABLE_HDF5)
    add_executable(test_HDF5 test/test_HDF5.C)
    target_link_libraries(test_HDF5 ${PROJECT_NAME})
    set_target_properties(test_HDF5 PROPERTIES COMPILE_FLAGS "${ChimeraTK-ApplicationCore_CXX_FLAGS}"
                                               LINK_FLAGS "${ChimeraTK-ApplicationCore_LINKER_FLAGS}")
    add_test(test_HDF5 test_HDF5)
  endif(ENABLE_HDF5)
  
  if(ENABLE_ROOT)
    add_executable(test_ScalarMicroDAQ test/testScalarMicroDAQ.C)
    target_link_libraries(test_ScalarMicroDAQ ${PROJECT_NAME} ROOT::Tree)
    set_target_properties(test_ScalarMicroDAQ PROPERTIES COMPILE_FLAGS "${ChimeraTK-ApplicationCore_CXX_FLAGS}"
                                                         LINK_FLAGS "${ChimeraTK-ApplicationCore_LINKER_FLAGS}")
    add_test( test_ScalarMicroDAQ test_ScalarMicroDAQ)                         
                              
    add_executable(test_ArrayMicroDAQ test/testArrayMicroDAQ.C)
    target_link_libraries(test_ArrayMicroDAQ ${PROJECT_NAME} ROOT::Tree)
    set_target_properties(test_ArrayMicroDAQ PROPERTIES COMPILE_FLAGS "${ChimeraTK-ApplicationCore_CXX_FLAGS}"
                                                        LINK_FLAGS "${ChimeraTK-ApplicationCore_LINKER_FLAGS}")
    add_test( test_ArrayMicroDAQ test_ArrayMicroDAQ)                             
  endif(ENABLE_ROOT)                                  
  
else()
  message(STATUS "Boost test framework not found. No boost based tests will be build.")
endif(Boost_UNIT_TEST_FRAMEWORK_FOUND AND ENABLE_TESTING)

if(ENABLE_ROOT)
  INSTALL(FILES ${PROJECT_BINARY_DIR}/lib${ROOTDICTDAQ}_rdict.pcm
          DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
          PERMISSIONS OWNER_READ OWNER_WRITE
                      GROUP_READ
                      WORLD_READ)
endif(ENABLE_ROOT)         

INSTALL(TARGETS  ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}LibraryDepends 
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER DESTINATION  ${CMAKE_INSTALL_PREFIX}/include/ChimeraTK/ApplicationCore
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
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
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)        
        
