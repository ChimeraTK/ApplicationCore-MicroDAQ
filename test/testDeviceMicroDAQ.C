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

#include "TChain.h"

#include <boost/test/included/unit_test.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/fusion/container/map.hpp>
using namespace boost::unit_test_framework;

struct TriggerModule : public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarPushInput<int32_t> trigger_in{this, "trigger_in", "", "trigger used in the test"};
  struct Out  :public ChimeraTK::VariableGroup{
    using ChimeraTK::VariableGroup::VariableGroup;
    ChimeraTK::ScalarOutput<int32_t> trigger{this, "trigger", "", "trigger used in the test", {"CS"}};
    ChimeraTK::ScalarOutput<int32_t> test{this, "test", "", "trigger used in the test", {"CS", "DAQ"}};
  } out {this, "Trigger", "", ChimeraTK::HierarchyModifier::moveToRoot};

  void mainLoop() override {
    while(true){
      out.trigger = (int32_t)trigger_in;
      out.test = (int32_t)trigger_in * 2;
      writeAll();
      readAll();
    }
  }
};

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
  TriggerModule trigger{this, "TriggerModule", ""};
  ChimeraTK::ConfigReader config{this, "Configuration", "device_test.xml"};
  ChimeraTK::ConnectingDeviceModule dev{this,"Dummy", "/Trigger/trigger"};
  ChimeraTK::MicroDAQ<int> daq{this,"MicroDAQ", "DAQ module", "DAQ", "/Trigger/trigger", ChimeraTK::HierarchyModifier::none};
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
    tf.writeScalar("/TriggerModule/trigger_in",(int)j);
    tf.writeScalar("/MyModule/actuator",(int)j);
    readback = (int)j;
    readback.write();
    tf.stepApplication();
  }
  TChain* ch = new TChain("data");
  ch->Add((app.dir+ "/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  int data[2];
  ch->SetBranchAddress("MyModule.actuator", &data[0]);
  ch->SetBranchAddress("MyModule.readback", &data[1]);
  ch->GetEvent(5);
  // initial value is 0, first value is 0, 5th entry is 3
  BOOST_CHECK_EQUAL(data[0], 3);
  BOOST_CHECK_EQUAL(data[1], 3);
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}
