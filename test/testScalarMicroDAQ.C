/*
 * testScalarMicroDAQ.C
 *
 *  Created on: Feb 3, 2020
 *      Author: Klaus Zenker (HZDR)
 */


//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE MicroDAQTest

#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>
#include <boost/thread.hpp>

#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include "ChimeraTK/ApplicationCore/Application.h"
#include "ChimeraTK/ApplicationCore/ControlSystemModule.h"
#include "ChimeraTK/ApplicationCore/ScalarAccessor.h"
#include "ChimeraTK/ApplicationCore/VariableNetworkNode.h"

#include "MicroDAQROOT.h"

#include "TChain.h"

#include <boost/test/unit_test.hpp>
#include <boost/fusion/container/map.hpp>
using namespace boost::unit_test_framework;

// list of user types the accessors are tested with
typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double> test_types;

template <typename UserType>
struct Dummy: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<UserType> out {this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};
  void mainLoop() override{
    out = 0;
    out.write();
    while(true){
      trigger.read();
      out = out + 1;
      out.write();
    }
  }
};

template <>
struct Dummy<std::string>: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<std::string> out {this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> out1 {this, "outTrigger", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};
  void mainLoop() override{
    out1.write();
    out.write();
    int i = 0;
    while(true){
      trigger.read();
      out = std::to_string(i);
      out.write();
      i++;
      writeAll();
    }
  }
};

/**
 * Define a test app to test the MicroDAQModule.
 */
template<typename UserType>
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test"){
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char *dir_name = mkdtemp(temName);
    dir = std::string(dir_name);
    // new fresh directory
    boost::filesystem::create_directory(dir);
  }
  ~testApp() { shutdown(); }

  ChimeraTK::ControlSystemModule cs;

  std::string dir;

  Dummy<UserType> module{this,"Dummy","Dummy module"};

  ChimeraTK::RootDAQ<UserType> daq;
//  ChimeraTK::MicroDAQ<int> daq{this,"MicroDAQ","Test", 10, 1000};

  void defineConnections() override {
    ChimeraTK::VariableNetworkNode trigger = cs["Config"]("trigger");
    trigger >> module.trigger;
    daq = ChimeraTK::RootDAQ<UserType>{this,"test","Test", 10, 1000};
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


template<>
struct testApp<std::string> : public ChimeraTK::Application {
  testApp() : Application("test"){
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char *dir_name = mkdtemp(temName);
    dir = std::string(dir_name);
    // new fresh directory
    boost::filesystem::create_directory(dir);
  }
  ~testApp() { shutdown(); }

  ChimeraTK::ControlSystemModule cs;

  std::string dir;

  Dummy<std::string> module{this,"Dummy","Dummy module"};

  ChimeraTK::RootDAQ<int> daq{this,"test","Test", 10, 1000};
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

BOOST_AUTO_TEST_CASE ( test_directory_access ){
  testApp<int32_t> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("enable", (int)1);
  tf.setScalarDefault("directory", (std::string)"/var/");
  tf.runApplication();

  tf.writeScalar("Config/trigger",(int)0);
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint32_t>("DAQError"), 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_dummy, T, test_types){
  testApp<T> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("enable", (int)1);
  tf.setScalarDefault("directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 10; j++){
    tf.writeScalar("Config/trigger",(int)j);
    tf.stepApplication();
  }

  TChain* ch = new TChain("test");
  ch->Add((app.dir+ "/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  T test;
  ch->SetBranchAddress("DAQ.out", &test);
  ch->GetEvent(4);
  BOOST_CHECK_EQUAL(test,4);

  delete ch;
  // remove currentBuffer and data0000.root to data0004.root and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}

BOOST_AUTO_TEST_CASE( test_dummy_str){
  testApp<std::string> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("enable", (int)1);
  tf.setScalarDefault("directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 10; j++){
    tf.writeScalar("Config/trigger",(int)j);
    tf.stepApplication();
  }

  TChain* ch = new TChain("test");
  ch->Add((app.dir+"/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  std::string* test = new std::string();
  ch->SetBranchAddress("DAQ.out", &test);
  ch->GetEvent(4);
  BOOST_CHECK_EQUAL(test->c_str(),"4");

  delete ch;
  delete test;
  // remove currentBuffer and data0000.root to data0004.root and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}
