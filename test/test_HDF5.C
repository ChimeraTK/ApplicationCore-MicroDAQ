/*
 * test_HDF5.C
 *
 *  Created on: Oct 17, 2017
 *      Author: Klaus Zenker (HZDR)
 */

#define BOOST_TEST_MODULE MicroDAQTestHDF5

#include "Dummy.h"
#include "H5Cpp.h"
#include "hdf5.h"
#include "MicroDAQHDF5.h"

#include <ChimeraTK/ApplicationCore/TestFacility.h>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/mpl/list.hpp>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

// this include must come last
#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;
#undef BOOST_NO_EXCEPTIONS

/********************************************************************************************************************/

/**
 * Define a test app to test the MicroDAQModule. Here it is possible to  specify the DAQ tag used.
 */
struct testAppTag : public ChimeraTK::Application {
  testAppTag(const std::string& tag = "DAQ") : Application("test"), module(this, "Dummy", "Dummy module", tag) {
    // cleanup from previous runs
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char* dir_name = mkdtemp(temName);
    dir = std::string(dir_name);

    // new fresh directory
    boost::filesystem::create_directory(dir);

    // add source
    daq.addSource("/Dummy", "DAQ");
  }

  ~testAppTag() override { shutdown(); }

  DummyWithTag module;

  ChimeraTK::HDF5DAQ<int> daq{this, "MicroDAQ", "Test of the MicroDAQ", 10, 1000, {}, "/Dummy/outTrigger"};

  std::string dir;
};

/********************************************************************************************************************/

/**
 * Define a test app to test the MicroDAQModule.
 */
template<typename UserType>
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test") {
    // cleanup from previous runs
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char* dir_name = mkdtemp(temName);
    dir = std::string(dir_name);

    // new fresh directory
    boost::filesystem::create_directory(dir);

    // add source
    daq.addSource("/Dummy", "DAQ");
  }

  ~testApp() override { shutdown(); }

  Dummy<UserType> module{this, "Dummy", "Dummy module"};

  ChimeraTK::HDF5DAQ<int> daq{this, "MicroDAQ", "Test of the MicroDAQ", 10, 1000, {}, "/Dummy/outTrigger"};

  std::string dir;
};

/********************************************************************************************************************/

/**
 * Define a test app to test the MicroDAQModule.
 */
template<typename UserType>
struct testAppArray : public ChimeraTK::Application {
  explicit testAppArray(uint32_t decimation = 10, uint32_t decimationThreshold = 1000)
  : Application("test"), _decimation(decimation), _decimationThreshold(decimationThreshold) {
    // cleanup from previous runs
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char* dir_name = mkdtemp(temName);
    dir = std::string(dir_name);

    // new fresh directory
    boost::filesystem::create_directory(dir);

    // add source
    daq.addSource("/Dummy", "DAQ");
  }

  ~testAppArray() override { shutdown(); }

  const uint32_t _decimation;
  const uint32_t _decimationThreshold;

  DummyArray<UserType> module{this, "Dummy", "Dummy module"};

  std::string dir;

  ChimeraTK::HDF5DAQ<int> daq{
      this, "MicroDAQ", "Test of the MicroDAQ", _decimation, _decimationThreshold, {}, "/Dummy/outTrigger"};
};

/********************************************************************************************************************/

#ifndef H5_NO_NAMESPACE
using namespace H5;
#endif

/********************************************************************************************************************/

BOOST_AUTO_TEST_CASE_TEMPLATE(test_scalar, T, test_types) {
  std::cout << "test_scalar<" << typeid(T).name() << ">" << std::endl;

  testApp<T> app;
  ChimeraTK::TestFacility tf(app);

  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", uint32_t(2));
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", uint32_t(5));
  tf.setScalarDefault("/MicroDAQ/activate", ChimeraTK::Boolean(true));

  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 9; j++) {
    tf.writeScalar("/Dummy/trigger", (int)j);
    // sleep in order not to produce data sets with the same name!
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
  auto dataGroup = event.openGroup("Dummy");
  DataSet dataset = dataGroup.openDataSet("out");
  DataSpace filespace = dataset.getSpace();
  hsize_t dims[1]; // dataset dimensions
  int rank = filespace.getSimpleExtentDims(dims);

  DataSpace mspace1(rank, dims);
  std::vector<float> v(1);
  dataset.read(&v[0], PredType::NATIVE_FLOAT, mspace1, filespace);
  if constexpr(std::is_same<T, bool>::value) {
    BOOST_CHECK_EQUAL(v.at(0), 0);
  }
  else {
    BOOST_CHECK_EQUAL(v.at(0), 2);
  }
  // remove currentBuffer and data0000.h5 to data0004.h5 and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}

/********************************************************************************************************************/

BOOST_AUTO_TEST_CASE_TEMPLATE(test_array, T, test_types) {
  std::cout << "test_array<" << typeid(T).name() << ">" << std::endl;

  testAppArray<T> app;
  ChimeraTK::TestFacility tf(app);

  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", uint32_t(2));
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", uint32_t(5));
  tf.setScalarDefault("/MicroDAQ/activate", ChimeraTK::Boolean(true));

  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 9; j++) {
    tf.writeScalar("/Dummy/trigger", (int)j);
    // sleep in order not to produce data sets with the same name!
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    tf.stepApplication();
  }

  // Only check second DAQ file
  boost::filesystem::path daqPath(app.dir);
  boost::filesystem::path file;
  if(boost::filesystem::exists(daqPath)) {
    if(boost::filesystem::is_directory(daqPath)) {
      for(auto i = boost::filesystem::directory_iterator(daqPath); i != boost::filesystem::directory_iterator(); i++) {
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
  auto dataGroup = event.openGroup("Dummy");
  DataSet dataset = dataGroup.openDataSet("out");
  DataSpace filespace = dataset.getSpace();
  hsize_t dims[1]; // dataset dimensions
  int rank = filespace.getSimpleExtentDims(dims);

  DataSpace mspace1(rank, dims);
  std::vector<float> v(10);
  dataset.read(&v[0], PredType::NATIVE_FLOAT, mspace1, filespace);

  if constexpr(std::is_same<T, bool>::value) {
    std::vector<float> v_test{1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    BOOST_CHECK_EQUAL_COLLECTIONS(v.begin(), v.end(), v_test.begin(), v_test.end());
  }
  else {
    std::vector<float> v_test{2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    BOOST_CHECK_EQUAL_COLLECTIONS(v.begin(), v.end(), v_test.begin(), v_test.end());
  }

  // remove currentBuffer and data0000.h5 to data0004.h5 and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}

/********************************************************************************************************************/

BOOST_AUTO_TEST_CASE(testWrongTag) {
  testAppTag app("WrongTag");
  ChimeraTK::TestFacility tf(app);
  BOOST_CHECK_THROW(tf.runApplication(), ChimeraTK::logic_error);
}

/********************************************************************************************************************/
