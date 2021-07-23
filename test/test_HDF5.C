/*
 * test_HDF5.C
 *
 *  Created on: Oct 17, 2017
 *      Author: zenker
 */

//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE MicroDAQTestHDF5

#include <boost/filesystem.hpp>
#include <boost/mpl/list.hpp>
#include <boost/format.hpp>

#include <string>
#include <vector>
#include "hdf5.h"
#include <stdlib.h>
#include <chrono>
#include <thread>


#include "H5Cpp.h"

#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include "ChimeraTK/ApplicationCore/Application.h"
#include "ChimeraTK/ApplicationCore/ControlSystemModule.h"
#include "ChimeraTK/ApplicationCore/ScalarAccessor.h"

#include "MicroDAQHDF5.h"

// this include must come last
#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;
#undef BOOST_NO_EXCEPTIONS

// list of user types the accessors are tested with
typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double> test_types;

template <typename UserType>
struct Dummy: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<UserType> out {this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};
  void mainLoop() override{
    out.write();
    while(true){
      trigger.read();
      out = out + 1;
      out.write();
    }
  }
};

/**
 * Define a test app to test the MicroDAQModule.
 */
template<typename UserType>
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test"){    // cleanup
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char *dir_name = mkdtemp(temName);
    dir = std::string(dir_name);
    // new fresh directory
    boost::filesystem::create_directory(dir);
  }
  ~testApp() {shutdown();}

  ChimeraTK::ControlSystemModule cs;

  Dummy<UserType> module{this,"Dummy","Dummy module"};

  ChimeraTK::HDF5DAQ<UserType> daq{this,"MicroDAQ","Test of the MicroDAQ"};

  std::string dir;

  void defineConnections() override {
    ChimeraTK::VariableNetworkNode trigger = cs["Config"]("trigger");
    trigger >> module.trigger;
//    daq = ChimeraTK::HDF5DAQ<UserType>{this,"MicroDAQ","Test of the MicroDAQ"};
    /**
     * Don't use the trigger for the microDAQ module. If doing so it might happen,
     * that the microDAQ module reads the latest value from the dummy module before it writes
     * its new value. In that case the test will be interrupted as the new value written
     * in the dummy module was not read by the microDAQ module.
     * If using the out variable of the dummy module as trigger it is ensured
     * that the latest value is read by the microDAQ module.
     */
    module.out >> daq.triggerGroup.trigger;
    daq.addSource(module.findTag("DAQ"),"DAQ");
    daq.connectTo(cs);

    dumpConnections();
  }

};

template <typename UserType>
struct DummyArray: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ArrayOutput<UserType> out {this, "out", "", 10, "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> out1 {this, "outTrigger", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};

  void mainLoop() override{
    out = {0,1,2,3,4,5,6,7,8,9};
    out.write();
    out1.write();
    while(true){
      trigger.read();
      std::transform(out.begin(), out.end(), out.begin(), [](UserType x){return x+1;});
      writeAll();
    }
  }
};

/**
 * Define a test app to test the MicroDAQModule.
 */
template<typename UserType>
struct testAppArray : public ChimeraTK::Application {
  testAppArray(uint32_t decimation = 10, uint32_t decimationThreshold = 1000) : Application("test"), _decimation(decimation), _decimationThreshold(decimationThreshold){
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char *dir_name = mkdtemp(temName);
    dir = std::string(dir_name);
    // new fresh directory
    boost::filesystem::create_directory(dir);
  }
  ~testAppArray() {shutdown();}

  const uint32_t _decimation;
  const uint32_t _decimationThreshold;

  ChimeraTK::ControlSystemModule cs;

  DummyArray<UserType> module{this,"Dummy","Dummy module"};

  std::string dir;

  ChimeraTK::HDF5DAQ<int> daq{this,"MicroDAQ","Test of the MicroDAQ", _decimation, _decimationThreshold};
//  ChimeraTK::MicroDAQ<int> daq{this,"MicroDAQ","Test", 10, 1000};

  void defineConnections() override {
    ChimeraTK::VariableNetworkNode trigger = cs["Config"]("trigger");
    trigger >> module.trigger;
    /**
     * Don't use the trigger for the microDAQ module. If doing so it might happen,
     * that the microDAQ module reads the latest value from the dummy module before it writes
     * its new value. In that case the test will be interrupted as the new value written
     * in the dummy module was not read by the microDAQ module.
     * If using the out variable of the dummy module as trigger it is ensured
     * that the latest value is read by the microDAQ module.
     */
    module.out1 >> daq.triggerGroup.trigger;
    daq.addSource(module.findTag("DAQ"),"DAQ");
    daq.connectTo(cs);


    dumpConnections();
  }

};

#ifndef H5_NO_NAMESPACE
    using namespace H5;
#endif

BOOST_AUTO_TEST_CASE_TEMPLATE( test_scalar, T, test_types){
  testApp<T> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 10; j++){
    tf.writeScalar("/Config/trigger", (int)j);
    // sleep in order not to produce data sets with the same name!
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    tf.stepApplication();
  }

  // Only check second DAQ file
  boost::filesystem::path daqPath(app.dir);
  boost::filesystem::path file;
  if (boost::filesystem::exists(daqPath)){
    if (boost::filesystem::is_directory(daqPath)){
      for(auto i = boost::filesystem::directory_iterator(daqPath); i != boost::filesystem::directory_iterator(); i++){
        // look for file *buffer0001.h5 -> that file includes data out=2 and out=3
        std::string match = (boost::format("buffer%04d%s") % 1 % ".h5").str();
        if(boost::filesystem::canonical(i->path()).string().find(match) != std::string::npos){
          file = i->path();
        }
      }
    }
  }

  // Only check first trigger
	H5File h5file( file.string().c_str(), H5F_ACC_RDONLY );
	Group gr = h5file.openGroup("/");
	auto event = gr.openGroup(gr.getObjnameByIdx(0).c_str());
	auto dataGroup = event.openGroup("DAQ");
  DataSet dataset = dataGroup.openDataSet( "out" );
  DataSpace filespace = dataset.getSpace();
  hsize_t dims[1];    // dataset dimensions
  int rank = filespace.getSimpleExtentDims(dims);

  DataSpace mspace1(rank, dims);
  std::vector<float> v(1);
  dataset.read( &v[0], PredType::NATIVE_FLOAT, mspace1, filespace );
  BOOST_CHECK_EQUAL(v.at(0),2);

  // remove currentBuffer and data0000.root to data0004.root and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_array, T, test_types){
  testAppArray<T> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 10; j++){
    tf.writeScalar("/Config/trigger", (int)j);
    // sleep in order not to produce data sets with the same name!
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    tf.stepApplication();
  }

  // Only check second DAQ file
  boost::filesystem::path daqPath(app.dir);
  boost::filesystem::path file;
  if (boost::filesystem::exists(daqPath)){
    if (boost::filesystem::is_directory(daqPath)){
      for(auto i = boost::filesystem::directory_iterator(daqPath); i != boost::filesystem::directory_iterator(); i++){
        std::string match = (boost::format("buffer%04d%s") % 1 % ".h5").str();
        if(boost::filesystem::canonical(i->path()).string().find(match) != std::string::npos){
          file = i->path();
        }
      }
    }
  }

  // Only check first trigger
  H5File h5file( file.string().c_str(), H5F_ACC_RDONLY );
  Group gr = h5file.openGroup("/");
  auto event = gr.openGroup(gr.getObjnameByIdx(0).c_str());
  auto dataGroup = event.openGroup("DAQ");
  DataSet dataset = dataGroup.openDataSet( "out" );
  DataSpace filespace = dataset.getSpace();
  hsize_t dims[1];    // dataset dimensions
  int rank = filespace.getSimpleExtentDims(dims);

  DataSpace mspace1(rank, dims);
  std::vector<float> v(10);
  dataset.read( &v[0], PredType::NATIVE_FLOAT, mspace1, filespace );

  std::vector<float> v_test{3,4,5,6,7,8,9,10,11,12};
  for(size_t i = 0; i < 10; i++){
    BOOST_CHECK_EQUAL(v.at(i),v_test.at(i));
  }

  // remove currentBuffer and data0000.root to data0004.root and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}
