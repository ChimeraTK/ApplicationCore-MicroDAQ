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
  } out {this, "Trigger", "", ChimeraTK::HierarchyModifier::moveToRoot};

  void mainLoop() override {
    while(true){
      out.trigger = (int32_t)trigger_in;
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
    ChimeraTK::setDMapFilePath("dummy.dmap");
  }
  ~testApp() { shutdown(); }

  std::string dir;

//  ChimeraTK::ConfigReader configReader{this, "Configuration", "device_test.xml", {"CONFIG", "CS"}};

  std::vector<ChimeraTK::ConnectingDeviceModule> cdevs;
  std::vector<ChimeraTK::MicroDAQ<int> > daq;
//  ChimeraTK::ConnectingDeviceModule dev{this,"Dummy", "/trigger"};

  TriggerModule triger{this, "Trigger Module", ""};

//  ChimeraTK::MicroDAQ<int> daq{this,"MicroDAQ", "DAQ module", "DAQ", "/trigger", ChimeraTK::HierarchyModifier::none, {"CS","MicroDAQ"}};

  void defineConnections() override {}
};


BOOST_AUTO_TEST_CASE ( test_device_daq ){
  testApp app;
  ChimeraTK::ControlSystemModule cs;
  app.findTag(".*").connectTo(cs);
//    daq.addDeviceModule(dev.getDeviceModule());
  ChimeraTK::ConnectingDeviceModule dev{&app,"Dummy", "/trigger"};
  app.cdevs.push_back(std::move(dev));

  ChimeraTK::TestFacility tf;
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/enable", (int)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  app.dumpConnections();
  tf.runApplication();

  for(size_t j = 0; j < 9; j++){
    tf.writeScalar("/trigger",(int)j);
    tf.stepApplication();
  }
  TChain* ch = new TChain("test");
  ch->Add((app.dir+ "/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}
