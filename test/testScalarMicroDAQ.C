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
//typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double> test_types;
typedef boost::mpl::list<int32_t> test_types;

template <typename UserType>
struct Dummy: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<UserType> out {this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger {this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};
  void mainLoop() override{
    out = 0;
    writeAll();
    while(true){
      trigger.read();
      out = out + 1;
      writeAll();
    }
  }
};

template <>
struct Dummy<std::string>: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<std::string> out {this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger {this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};
  void mainLoop() override{
    // The first string will be initalized to "" instead of "0", but we don't care here
    writeAll();
    // Set to 1 so that the second entry will be "1"
    int i = 1;
    while(true){
      trigger.read();
      out = std::to_string(i);
      writeAll();
      ++i;
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

  std::string dir;

  Dummy<UserType> module{this,"Dummy","Dummy module"};

  ChimeraTK::RootDAQ<int> daq{this,"MicroDAQ","Test", 10, 1000, ChimeraTK::HierarchyModifier::none, {} , "/Dummy/outTrigger", "test"};

  void defineConnections() override {
    daq.addSource(module.findTag("DAQ"),"DAQ");
    ChimeraTK::ControlSystemModule cs;
    findTag(".*").connectTo(cs);
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

  std::string dir;

  Dummy<std::string> module{this,"Dummy","Dummy module"};

  ChimeraTK::RootDAQ<int> daq{this,"MicroDAQ","Test", 10, 1000, ChimeraTK::HierarchyModifier::none, {} , "/Dummy/outTrigger", "test"};

  void defineConnections() override {
    daq.addSource(module.findTag("DAQ"),"DAQ");
    ChimeraTK::ControlSystemModule cs;
    findTag(".*").connectTo(cs);
    dumpConnections();
  }

};

BOOST_AUTO_TEST_CASE ( test_directory_access ){
  testApp<int32_t> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", (std::string)"/var/");
  tf.runApplication();

  tf.writeScalar("/Dummy/trigger",(int)0);
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint32_t>("/MicroDAQ/DAQError"), 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_dummy, T, test_types){
  testApp<T> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 9; j++){
    tf.writeScalar("/Dummy/trigger",(int)j);
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
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 9; j++){
    tf.writeScalar("/Dummy/trigger",(int)j);
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
