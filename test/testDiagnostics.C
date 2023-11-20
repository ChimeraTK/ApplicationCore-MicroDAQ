// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * testDiagnostics.C
 *
 *  Created on: Nov 17, 2023
 *      Author: Klaus Zenker (HZDR)
 */

// #define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE MicroDAQTest

#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include "Dummy.h"
#include "MicroDAQROOT.h"
#include "TChain.h"

#include <chrono>
#include <fstream>

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;
#undef BOOST_NO_EXCEPTIONS

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

    // add source
    daq.addSource("/Dummy", "DAQ");
  }
  ~testApp() { shutdown(); }

  std::string dir;

  Dummy<UserType> module{this, "Dummy", "Dummy module"};

  ChimeraTK::RootDAQ<int> daq{this, "MicroDAQ", "Test", 10, 1000, {}, "/Dummy/outTrigger", "test"};
};

BOOST_AUTO_TEST_CASE(test_directory_access) {
  testApp<int32_t> app;
  ChimeraTK::TestFacility tf(app);
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/activate", (ChimeraTK::Boolean)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  tf.writeScalar("/Dummy/trigger", (int)0);
  tf.stepApplication();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  tf.writeScalar("/Dummy/trigger", (int)2);
  tf.stepApplication();
  BOOST_CHECK_EQUAL(tf.readScalar<int32_t>("/MicroDAQ/status/nMissedTriggers"), 1);
  // should be at least 200ms
  BOOST_CHECK_GE(tf.readScalar<int64_t>("/MicroDAQ/status/triggerPeriod"), 199);
}
