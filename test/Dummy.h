// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * Dummy.h
 *
 *  Created on: Sep 7, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include "MicroDAQ.h"

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

#include <boost/mpl/list.hpp>

typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double, bool, std::string>
    test_types;

/********************************************************************************************************************/

struct DummyWithTag : public ChimeraTK::ApplicationModule {
  DummyWithTag(ChimeraTK::ModuleGroup* module, const std::string& name, const std::string& description,
      const std::string& tag = "DAQ")
  : ChimeraTK::ApplicationModule(module, name, description), out(this, "out", "", "Dummy output", {tag}){};

  ChimeraTK::ScalarOutput<int> out;
  ChimeraTK::ScalarOutput<int> outTrigger{this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger{this, "trigger", "", "Trigger", {}};

  void mainLoop() override {
    out = 0;
    // This also writes outTrigger!
    writeAll();
    while(true) {
      trigger.read();
      out = out + 1;
      writeAll();
    }
  }
};

/********************************************************************************************************************/

template<typename UserType>
struct Dummy : public ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;

  ChimeraTK::ScalarOutput<UserType> out{this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger{this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger{this, "trigger", "", "Trigger", {}};

  void mainLoop() override {
    out = 0;
    // This also writes outTrigger!
    writeAll();
    while(true) {
      trigger.read();
      out = out + 1;
      outTrigger = (int)trigger;
      writeAll();
    }
  }
};

/********************************************************************************************************************/

template<>
struct Dummy<std::string> : public ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<std::string> out{this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger{this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger{this, "trigger", "", "Trigger", {}};
  void mainLoop() override {
    // The first string will be initalized to "" instead of "0", but we don't care here
    writeAll();
    // Set to 1 so that the second entry will be "1"
    int i = 1;
    while(true) {
      trigger.read();
      out = std::to_string(i);
      outTrigger = (int)trigger;
      writeAll();
      ++i;
    }
  }
};

/********************************************************************************************************************/

template<>
struct Dummy<bool> : public ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<ChimeraTK::Boolean> out{this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger{this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger{this, "trigger", "", "Trigger", {}};
  void mainLoop() override {
    // The first string will be initalized to "" instead of "0", but we don't care here
    writeAll();
    out = false;
    while(true) {
      trigger.read();
      out = !out;
      outTrigger = (int)trigger;
      writeAll();
    }
  }
};

/********************************************************************************************************************/

template<typename UserType>
struct DummyArray : public ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ArrayOutput<UserType> out{this, "out", "", 10, "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger{this, "outTrigger", "", "DAQ trigger", {"DAQ"}};
  ChimeraTK::ScalarPushInput<int> trigger{this, "trigger", "", "Trigger", {}};

  void mainLoop() override {
    out = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    out.write();
    outTrigger.write();
    while(true) {
      trigger.read();
      std::transform(out.begin(), out.end(), out.begin(), [](UserType x) { return x + 1; });
      outTrigger = outTrigger + 1;
      writeAll();
    }
  }
};

/********************************************************************************************************************/

template<>
struct DummyArray<std::string> : public ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ArrayOutput<std::string> out{this, "out", "", 10, "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger{this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger{this, "trigger", "", "Trigger", {}};
  void mainLoop() override {
    out = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    writeAll();
    while(true) {
      trigger.read();
      std::transform(
          out.begin(), out.end(), out.begin(), [](std::string val) { return std::to_string(std::stoi(val) + 1); });
      out.write();
      // here also out1 is written. No reason for setting a certain value
      writeAll();
    }
  }
};

/********************************************************************************************************************/

template<>
struct DummyArray<bool> : public ChimeraTK::ApplicationModule {
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ArrayOutput<ChimeraTK::Boolean> out{this, "out", "", 10, "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger{this, "outTrigger", "", "DAQ trigger", {"DAQ"}};
  ChimeraTK::ScalarPushInput<int> trigger{this, "trigger", "", "Trigger", {}};

  void mainLoop() override {
    out = {true, false, true, false, true, false, true, false, true, false};
    out.write();
    outTrigger.write();
    while(true) {
      trigger.read();
      std::transform(out.begin(), out.end(), out.begin(), [](bool x) { return !x; });
      outTrigger = outTrigger + 1;
      writeAll();
    }
  }
};

/********************************************************************************************************************/

/**
 * Define a test app to test adding device modules to the MicroDAQ module
 */
struct DeviceDummyApp : public ChimeraTK::Application {
  DeviceDummyApp(const std::string& configFile, const std::string& namePrefix = "", const std::string& subModule = "")
  : Application("test"), config(this, "Configuration", configFile) {
    // cleanup from previous run
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char* dir_name = mkdtemp(temName);
    dir = std::string(dir_name);

    // new fresh directory
    boost::filesystem::create_directory(dir);

    // add device as source
    daq.addDeviceModule(dev, namePrefix, subModule);
  }
  ~DeviceDummyApp() override { shutdown(); }

  ChimeraTK::SetDMapFilePath dmap{"dummy.dmap"};

  std::string dir;

  // somehow without an module the application does not start...
  Dummy<int32_t> module{this, "Dummy", "Module used as trigger"};
  ChimeraTK::ConfigReader config;
  ChimeraTK::DeviceModule dev{this, "Dummy", "/Dummy/outTrigger"};
  ChimeraTK::MicroDAQ<int> daq{this, "MicroDAQ", "DAQ module", "DAQ", "/Dummy/outTrigger"};
};
