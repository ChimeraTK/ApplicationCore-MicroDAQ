/*
 * testScalarMicroDAQ.C
 *
 *  Created on: Feb 3, 2020
 *      Author: Klaus Zenker (HZDR)
 */

//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE MicroDAQTest

#include <fstream>
#include <type_traits>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>

#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include "ChimeraTK/ApplicationCore/ControlSystemModule.h"

#include "MicroDAQROOT.h"
#include "Dummy.h"

#include "TChain.h"

#include <boost/test/unit_test.hpp>
#include <boost/fusion/container/map.hpp>
using namespace boost::unit_test_framework;

/**
 * Define a test app to test the MicroDAQModule.
 */
template<typename UserType>
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test") {
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char* dir_name = mkdtemp(temName);
    dir = std::string(dir_name);
    // new fresh directory
    boost::filesystem::create_directory(dir);
  }
  ~testApp() { shutdown(); }

  std::string dir;

  Dummy<UserType> module{this, "Dummy", "Dummy module"};

  ChimeraTK::RootDAQ<int> daq{
      this, "MicroDAQ", "Test", 10, 1000, ChimeraTK::HierarchyModifier::none, {}, "/Dummy/outTrigger", "test"};

  void defineConnections() override {
    daq.addSource(module.findTag("DAQ"), "DAQ");
    ChimeraTK::ControlSystemModule cs;
    findTag(".*").connectTo(cs);
    dumpConnections();
  }
};

BOOST_AUTO_TEST_CASE(test_directory_access) {
  testApp<int32_t> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", (std::string) "/NonExistingFolder/");
  tf.runApplication();

  tf.writeScalar("/Dummy/trigger", (int)0);
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<uint32_t>("/MicroDAQ/DAQError"), 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_dummy, T, test_types) {
  testApp<T> app;
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 9; j++) {
    tf.writeScalar("/Dummy/trigger", (int)j);
    tf.stepApplication();
  }

  TChain* ch = new TChain("test");
  ch->Add((app.dir + "/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  T* ptest = new T();
  T test;
  if constexpr(std::is_same<T, std::string>::value) {
    ch->SetBranchAddress("DAQ.out", &ptest);
  }
  else {
    ch->SetBranchAddress("DAQ.out", &test);
  }
  ch->GetEvent(4);
  if constexpr(std::is_same<T, bool>::value) {
    BOOST_CHECK_EQUAL(test, false);
  }
  else if constexpr(std::is_same<T, std::string>::value) {
    BOOST_CHECK_EQUAL(ptest->c_str(), "4");
  }
  else {
    BOOST_CHECK_EQUAL(test, 4);
  }
  ch->GetEvent(5);
  if constexpr(std::is_same<T, bool>::value) {
    BOOST_CHECK_EQUAL(test, true);
  }
  else if constexpr(std::is_same<T, std::string>::value) {
    BOOST_CHECK_EQUAL(ptest->c_str(), "5");
  }
  else {
    BOOST_CHECK_EQUAL(test, 5);
  }
  delete ch;
  delete ptest;
  // remove currentBuffer and data0000.root to data0004.root and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}
