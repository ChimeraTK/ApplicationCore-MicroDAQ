enable_testing()
FILE(COPY dummy.dmap
        dummy.map
        dummy.xlmap
        device_test_ROOT.xml
        device_test_HDF5.xml
   DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
if(ENABLE_HDF5)
  add_executable(test_HDF5 test_HDF5.C ${test_headers})
  target_link_libraries(test_HDF5 ${PROJECT_NAME} ${HDF5_HL_LIBRARIES} ${HDF5_CXX_LIBRARIES})
  add_test(test_HDF5 test_HDF5)
  
  FILE(COPY device_test_HDF5.xml DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  add_executable(test_Device_HDF5 testDevice_HDF5.C ${test_headers})
  target_link_libraries(test_Device_HDF5 ${PROJECT_NAME} ${HDF5_HL_LIBRARIES} ${HDF5_CXX_LIBRARIES} ChimeraTK::ChimeraTK-ApplicationCore)
  add_test( test_Device_HDF5 test_Device_HDF5)     
endif(ENABLE_HDF5)

if(ENABLE_ROOT)
  add_executable(test_ScalarMicroDAQ testScalarMicroDAQ.C ${test_headers})
  target_link_libraries(test_ScalarMicroDAQ ${PROJECT_NAME} ROOT::Tree)
  add_test( test_ScalarMicroDAQ test_ScalarMicroDAQ)                         
                            
  add_executable(test_ArrayMicroDAQ testArrayMicroDAQ.C ${test_headers})
  target_link_libraries(test_ArrayMicroDAQ ${PROJECT_NAME} ROOT::Tree ChimeraTK::ChimeraTK-ApplicationCore)
  add_test( test_ArrayMicroDAQ test_ArrayMicroDAQ)                             

  FILE(COPY device_test_ROOT.xml DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

  add_executable(test_Device_ROOT testDevice_ROOT.C ${test_headers})
  target_link_libraries(test_Device_ROOT ${PROJECT_NAME} ROOT::Tree ChimeraTK::ChimeraTK-ApplicationCore)
  add_test( test_Device_ROOT test_Device_ROOT)
  
  add_executable(test_Diagnostics testDiagnostics.C)
  target_link_libraries(test_Diagnostics ${PROJECT_NAME} ROOT::Tree ChimeraTK::ChimeraTK-ApplicationCore)
  add_test( test_Diagnostics test_Device_ROOT)                                 


endif(ENABLE_ROOT)                                  
