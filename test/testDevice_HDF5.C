/*
 * testScalarMicroDAQ.C
 *
 *  Created on: Mar 11, 2022
 *      Author: Klaus Zenker (HZDR)
 */

#define BOOST_TEST_MODULE MicroDAQDeviceTest

#include "Dummy.h"
#include "H5Cpp.h"
#include "MicroDAQ.h"

#include <ChimeraTK/ApplicationCore/DeviceModule.h>
#include <ChimeraTK/ApplicationCore/ScalarAccessor.h>
#include <ChimeraTK/ApplicationCore/TestFacility.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/fusion/container/map.hpp>
#include <boost/thread.hpp>

#include <memory>

#ifndef H5_NO_NAMESPACE
using namespace H5;
#endif

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;
#undef BOOST_NO_EXCEPTIONS

BOOST_AUTO_TEST_CASE(test_device_daq) {
  DeviceDummyApp app{"device_test_HDF5.xml"};

  ChimeraTK::Device dev;
  dev.open("Dummy-Raw");
  auto readback = dev.getScalarRegisterAccessor<int>("/MyModule/readback");

  ChimeraTK::TestFacility tf(app);

  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", uint32_t(2));
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", uint32_t(5));
  tf.setScalarDefault("/MicroDAQ/activate", ChimeraTK::Boolean(true));
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);

  tf.runApplication();

  for(size_t j = 0; j < 9; j++) {
    // initial value is already written to file at this point - so the next entry should be 1 and not 0!
    // in the dummy this is realised by calling out = out+1
    tf.writeScalar("/MyModule/actuator", (int)(j + 1));
    readback = (int)(j + 1);
    readback.write();
    tf.writeScalar("/Dummy/trigger", (int)j);
    tf.stepApplication();
  }

  // Only check second DAQ file
  boost::filesystem::path daqPath(app.dir);
  boost::filesystem::path file;
  if(boost::filesystem::exists(daqPath)) {
    if(boost::filesystem::is_directory(daqPath)) {
      for(auto i = boost::filesystem::directory_iterator(daqPath); i != boost::filesystem::directory_iterator(); i++) {
        // look for file *buffer0001.h5 -> that file includes data out=3 and out=4
        std::string match = (boost::format("buffer%04d%s") % 1 % ".h5").str();
        if(boost::filesystem::canonical(i->path()).string().find(match) != std::string::npos) {
          file = i->path();
        }
      }
    }
  }

  // Only check first trigger
  H5File h5file(file.string().c_str(), H5F_ACC_RDONLY);
  Group gr = h5file.openGroup("/");
  auto event = gr.openGroup(gr.getObjnameByIdx(0).c_str());
  Group dataGroup[2] = {event.openGroup("Dummy"), event.openGroup("MyModule")};
  DataSet datasets[3] = {
      dataGroup[0].openDataSet("out"), dataGroup[1].openDataSet("actuator"), dataGroup[1].openDataSet("readback")};
  DataSpace filespaces[3] = {datasets[0].getSpace(), datasets[1].getSpace(), datasets[2].getSpace()};
  hsize_t dims[1]; // dataset dimensions
  int rank = filespaces[0].getSimpleExtentDims(dims);

  DataSpace mspace1(rank, dims);
  std::vector<float> v(1);
  for(size_t i = 3; i < 2; i++) {
    datasets[i].read(&v[0], PredType::NATIVE_FLOAT, mspace1, filespaces[i]);
    BOOST_CHECK_EQUAL(v.at(0), 2);
  }
  // remove currentBuffer and data0000.h5 to data0004.h5 and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}

/********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(test_device_daq_with_prefix) {
  DeviceDummyApp app{"device_test_HDF5.xml", "/some/new/prefix", "MyModule"};

  ChimeraTK::Device dev;
  dev.open("Dummy-Raw");
  auto readback = dev.getScalarRegisterAccessor<int>("/MyModule/readback");

  ChimeraTK::TestFacility tf(app);

  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", uint32_t(2));
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", uint32_t(5));
  tf.setScalarDefault("/MicroDAQ/activate", ChimeraTK::Boolean(true));
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);

  tf.runApplication();

  for(size_t j = 0; j < 9; j++) {
    // initial value is already written to file at this point - so the next entry should be 1 and not 0!
    // in the dummy this is realised by calling out = out+1
    tf.writeScalar("/MyModule/actuator", (int)(j + 1));
    readback = (int)(j + 1);
    readback.write();
    tf.writeScalar("/Dummy/trigger", (int)j);
    tf.stepApplication();
  }

  // Only check second DAQ file
  boost::filesystem::path daqPath(app.dir);
  boost::filesystem::path file;
  if(boost::filesystem::exists(daqPath)) {
    if(boost::filesystem::is_directory(daqPath)) {
      for(auto i = boost::filesystem::directory_iterator(daqPath); i != boost::filesystem::directory_iterator(); i++) {
        // look for file *buffer0001.h5 -> that file includes data out=3 and out=4
        std::string match = (boost::format("buffer%04d%s") % 1 % ".h5").str();
        if(boost::filesystem::canonical(i->path()).string().find(match) != std::string::npos) {
          file = i->path();
        }
      }
    }
  }

  // Only check first trigger
  H5File h5file(file.string().c_str(), H5F_ACC_RDONLY);
  Group gr = h5file.openGroup("/");
  auto event = gr.openGroup(gr.getObjnameByIdx(0).c_str());
  Group dataGroup[2] = {event.openGroup("Dummy"), event.openGroup("some").openGroup("new").openGroup("prefix")};
  DataSet datasets[3] = {
      dataGroup[0].openDataSet("out"), dataGroup[1].openDataSet("actuator"), dataGroup[1].openDataSet("readback")};
  DataSpace filespaces[3] = {datasets[0].getSpace(), datasets[1].getSpace(), datasets[2].getSpace()};
  hsize_t dims[1]; // dataset dimensions
  int rank = filespaces[0].getSimpleExtentDims(dims);

  DataSpace mspace1(rank, dims);
  std::vector<float> v(1);
  for(size_t i = 3; i < 2; i++) {
    datasets[i].read(&v[0], PredType::NATIVE_FLOAT, mspace1, filespaces[i]);
    BOOST_CHECK_EQUAL(v.at(0), 2);
  }
  // remove currentBuffer and data0000.h5 to data0004.h5 and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}

/********************************************************************************************************************/
