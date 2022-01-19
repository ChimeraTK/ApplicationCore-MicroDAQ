/*
 * Dummy.h
 *
 *  Created on: Sep 7, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include <boost/mpl/list.hpp>

#include <ChimeraTK/ApplicationCore/ApplicationCore.h>

typedef boost::mpl::list<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, float, double, bool, std::string>
    test_types;

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

template <>
struct Dummy<bool>: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ScalarOutput<ChimeraTK::Boolean> out {this, "out", "", "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger {this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};
  void mainLoop() override{
    // The first string will be initalized to "" instead of "0", but we don't care here
    writeAll();
    // Set to 1 so that the second entry will be "1"
    int i = 1;
    out = false;
    while(true){
      trigger.read();
      out = !out;
      writeAll();
      ++i;
    }
  }
};

template <typename UserType>
struct DummyArray: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ArrayOutput<UserType> out {this, "out", "", 10, "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger {this, "outTrigger", "", "DAQ trigger", {"DAQ"}};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};

  void mainLoop() override{
    out = {0,1,2,3,4,5,6,7,8,9};
    out.write();
    outTrigger.write();
    while(true){
      trigger.read();
      std::transform(out.begin(), out.end(), out.begin(), [](UserType x){return x+1;});
      outTrigger =  outTrigger + 1;
      writeAll();
    }
  }
};

template <>
struct DummyArray<std::string>: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ArrayOutput<std::string> out {this, "out", "", 10, "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger {this, "outTrigger", "", "Dummy output"};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};
  void mainLoop() override{
    out = {"0","1","2","3","4","5","6","7","8","9"};
    writeAll();
    while(true){
      trigger.read();
      std::transform(out.begin(), out.end(), out.begin(), [](std::string val){ return std::to_string(std::stoi(val) + 1);});
      out.write();
      // here also out1 is written. No reason for setting a certain value
      writeAll();
    }
  }
};

template <>
struct DummyArray<bool>: public ChimeraTK::ApplicationModule{
  using ChimeraTK::ApplicationModule::ApplicationModule;
  ChimeraTK::ArrayOutput<ChimeraTK::Boolean> out {this, "out", "", 10, "Dummy output", {"DAQ"}};
  ChimeraTK::ScalarOutput<int> outTrigger {this, "outTrigger", "", "DAQ trigger", {"DAQ"}};
  ChimeraTK::ScalarPushInput<int> trigger {this, "trigger", "" ,"Trigger", {}};

  void mainLoop() override{
    out = {true,false,true,false,true,false,true,false,true,false};
    out.write();
    outTrigger.write();
    while(true){
      trigger.read();
      std::transform(out.begin(), out.end(), out.begin(), [](bool x){return !x;});
      outTrigger =  outTrigger + 1;
      writeAll();
    }
  }
};

