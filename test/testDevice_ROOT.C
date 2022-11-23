/*
 * testScalarMicroDAQ.C
 *
 *  Created on: Mar 11, 2022
 *      Author: Klaus Zenker (HZDR)
 */

#define BOOST_TEST_MODULE MicroDAQDeviceTest

#include "ChimeraTK/ApplicationCore/DeviceModule.h"
#include "ChimeraTK/ApplicationCore/ScalarAccessor.h"
#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include "Dummy.h"
#include "H5Cpp.h"
#include "MicroDAQROOT.h"
#include "TChain.h"

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/fusion/container/map.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

#include <memory>
using namespace boost::unit_test_framework;

BOOST_AUTO_TEST_CASE(test_device_daq) {
  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("dummy.dmap");
  DeviceDummyApp app("device_test_ROOT.xml");

  ChimeraTK::Device dev;
  dev.open("Dummy-Raw");
  auto readback = dev.getScalarRegisterAccessor<int>("/MyModule/readback");
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/activate", (ChimeraTK::Boolean)1);
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
  TChain* ch = new TChain("data");
  ch->Add((app.dir + "/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  int data[3];
  ch->SetBranchAddress("MyModule.actuator", &data[0]);
  ch->SetBranchAddress("MyModule.readback", &data[1]);
  ch->SetBranchAddress("Dummy.out", &data[2]);
  ch->GetEvent(5);

  BOOST_CHECK_EQUAL(data[0], 5);
  BOOST_CHECK_EQUAL(data[1], 5);
  BOOST_CHECK_EQUAL(data[2], 5);
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}
