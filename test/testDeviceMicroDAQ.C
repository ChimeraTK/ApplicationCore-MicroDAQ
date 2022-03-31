/*
 * testScalarMicroDAQ.C
 *
 *  Created on: Mar 11, 2022
 *      Author: Klaus Zenker (HZDR)
 */

#define BOOST_TEST_MODULE MicroDAQDeviceTest

#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include "ChimeraTK/ApplicationCore/ControlSystemModule.h"
#include "ChimeraTK/ApplicationCore/DeviceModule.h"
#include "ChimeraTK/ApplicationCore/ScalarAccessor.h"


#include "MicroDAQROOT.h"
#include "Dummy.h"

#include "TChain.h"

#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/fusion/container/map.hpp>
using namespace boost::unit_test_framework;

/**
 * Define a test app to test the MicroDAQModule.
 */
struct testApp : public ChimeraTK::Application {
  testApp() : Application("test"){
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char *dir_name = mkdtemp(temName);
    dir = std::string(dir_name);
    // new fresh directory
    boost::filesystem::create_directory(dir);
  }
  ~testApp() override { shutdown(); }

  std::string dir;
  // somehow without an module the application does not start...
  Dummy<int32_t> module{this, "Dummy", "Module used as trigger"};
  ChimeraTK::ConfigReader config{this, "Configuration", "device_test.xml"};
  ChimeraTK::ConnectingDeviceModule dev{this,"Dummy", "/Dummy/outTrigger"};
  ChimeraTK::MicroDAQ<int> daq{this,"MicroDAQ", "DAQ module", "DAQ", "/Dummy/outTrigger", ChimeraTK::HierarchyModifier::none};
  void defineConnections() override {
    ChimeraTK::ControlSystemModule cs;
    findTag(".*").connectTo(cs);
    daq.addDeviceModule(dev.getDeviceModule());
  }
  void initialise() override {
    Application::initialise();
    dumpConnections();
  }
};


BOOST_AUTO_TEST_CASE ( test_device_daq ){
  ChimeraTK::BackendFactory::getInstance().setDMapFilePath("dummy.dmap");
  testApp app;

  ChimeraTK::Device dev;
  dev.open("Dummy-Raw");
  auto readback = dev.getScalarRegisterAccessor<int>("/MyModule/readback");
  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();
  for(size_t j = 0; j < 9; j++){
    // initial value is already written to file at this point - so the next entry should be 1 and not 0!
    // in the dummy this is realised by calling out = out+1
    tf.writeScalar("/MyModule/actuator",(int)(j+1));
    readback = (int)(j+1);
    readback.write();
    tf.writeScalar("/Dummy/trigger",(int)j);
    tf.stepApplication();
  }
  TChain* ch = new TChain("data");
  ch->Add((app.dir+ "/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  int data[3];
  ch->SetBranchAddress("MyModule.actuator", &data[0]);
  ch->SetBranchAddress("MyModule.readback", &data[1]);
  ch->SetBranchAddress("Dummy.out", &data[2]);
  ch->GetEvent(5);

  for(size_t i =0; i < 3; i++)
    BOOST_CHECK_EQUAL(data[i], 5);
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}
