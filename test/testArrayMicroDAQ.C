// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * testArrayMicroDAQ.C
 *
 *  Created on: Feb 24, 2020
 *      Author: Klaus Zenker (HZDR)
 */

// #define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE MicroDAQTest

#include "ChimeraTK/ApplicationCore/TestFacility.h"
#include "data_types.h"
#include "Dummy.h"
#include "MicroDAQROOT.h"
#include "TChain.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/fusion/container/map.hpp>
#include <boost/thread.hpp>

#include <algorithm>
#include <fstream>

#define BOOST_NO_EXCEPTIONS
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test_framework;
#undef BOOST_NO_EXCEPTIONS

/**
 * Define a test app to test the MicroDAQModule.
 */
template<typename UserType>
struct testAppArray : public ChimeraTK::Application {
  testAppArray(uint32_t decimation = 10, uint32_t decimationThreshold = 1000)
  : Application("test"), _decimation(decimation), _decimationThreshold(decimationThreshold) {
    char temName[] = "/tmp/uDAQ.XXXXXX";
    char* dir_name = mkdtemp(temName);
    dir = std::string(dir_name);
    // new fresh directory
    boost::filesystem::create_directory(dir);

    // add source
    daq.addSource("/Dummy", "DAQ");
  }
  ~testAppArray() override { shutdown(); }

  const uint32_t _decimation;
  const uint32_t _decimationThreshold;

  std::string dir;

  DummyArray<UserType> module{this, "Dummy", "Dummy module"};

  ChimeraTK::RootDAQ<int> daq{
      this, "MicroDAQ", "Test", _decimation, _decimationThreshold, {}, "/Dummy/outTrigger", "test"};
};

typedef boost::fusion::map<boost::fusion::pair<int8_t, size_t>, boost::fusion::pair<uint8_t, size_t>,
    boost::fusion::pair<int16_t, size_t>, boost::fusion::pair<uint16_t, size_t>, boost::fusion::pair<int32_t, size_t>,
    boost::fusion::pair<uint32_t, size_t>, boost::fusion::pair<int64_t, size_t>, boost::fusion::pair<uint64_t, size_t>,
    boost::fusion::pair<float, size_t>, boost::fusion::pair<double, size_t>, boost::fusion::pair<bool, size_t>,
    boost::fusion::pair<std::string, size_t>>
    myMap;

myMap m(boost::fusion::make_pair<int8_t>(0), boost::fusion::make_pair<uint8_t>(0), boost::fusion::make_pair<int16_t>(0),
    boost::fusion::make_pair<uint16_t>(0), boost::fusion::make_pair<int32_t>(1), boost::fusion::make_pair<uint32_t>(1),
    boost::fusion::make_pair<int64_t>(2), boost::fusion::make_pair<uint64_t>(2), boost::fusion::make_pair<float>(3),
    boost::fusion::make_pair<double>(4), boost::fusion::make_pair<bool>(5), boost::fusion::make_pair<std::string>(6));

BOOST_AUTO_TEST_CASE_TEMPLATE(test_dummy_array, T, test_types) {
  testAppArray<T> app;
  ChimeraTK::TestFacility tf(app);
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/activate", (ChimeraTK::Boolean)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 9; j++) {
    tf.writeScalar("/Dummy/trigger", (int)j);
    tf.stepApplication();
  }

  size_t t = boost::fusion::at_key<T>(m);
  std::shared_ptr<TChain> ch(new TChain("test"));
  ch->Add((app.dir + "/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  TArray* arr = nullptr;
  std::shared_ptr<TArrayS> as(new TArrayS());
  std::shared_ptr<TArrayI> ai(new TArrayI());
  std::shared_ptr<TArrayL> al(new TArrayL());
  std::shared_ptr<TArrayF> af(new TArrayF());
  std::shared_ptr<TArrayD> ad(new TArrayD());
  std::shared_ptr<TArrayC> ac(new TArrayC());
  std::shared_ptr<ChimeraTK::detail::TArrayStr> astr(new ChimeraTK::detail::TArrayStr());
  if(t == 0) {
    arr = as.get();
    auto p = as.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 1) {
    arr = ai.get();
    auto p = ai.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 2) {
    arr = al.get();
    auto p = al.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 3) {
    arr = af.get();
    auto p = af.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 4) {
    arr = ad.get();
    auto p = ad.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 5) {
    arr = ac.get();
    auto p = ac.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 6) {
    arr = astr.get();
    auto p = astr.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  ch->GetEvent(4);
  if constexpr(std::is_same<T, bool>::value) {
    std::vector<bool> test{true, false, true, false, true, false, true, false, true, false};
    for(size_t i = 0; i < 10; i++) {
      BOOST_CHECK_EQUAL(arr->GetAt(i), test.at(i));
    }
  }
  else if constexpr(std::is_same<T, std::string>::value) {
    ChimeraTK::detail::TArrayStr* strarr = static_cast<ChimeraTK::detail::TArrayStr*>(arr);
    for(size_t i = 0; i < 10; i++) {
      BOOST_CHECK_EQUAL(strarr->At(i), std::to_string(i + 4));
    }
  }
  else {
    // array is 4,5,6,7,8,9,10,11,12,13
    for(size_t i = 0; i < 10; i++) {
      BOOST_CHECK_EQUAL(arr->GetAt(i), i + 4);
    }
  }

  // remove currentBuffer and data0000.root to data0004.root and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_decimation, T, test_types) {
  testAppArray<T> app(2, 5);
  ChimeraTK::TestFacility tf(app);
  tf.setScalarDefault("/MicroDAQ/nTriggersPerFile", (uint32_t)2);
  tf.setScalarDefault("/MicroDAQ/nMaxFiles", (uint32_t)5);
  tf.setScalarDefault("/MicroDAQ/activate", (ChimeraTK::Boolean)1);
  tf.setScalarDefault("/MicroDAQ/directory", app.dir);
  tf.runApplication();

  for(size_t j = 0; j < 9; j++) {
    tf.writeScalar("/Dummy/trigger", (int)j);
    tf.stepApplication();
  }
  size_t t = boost::fusion::at_key<T>(m);
  std::shared_ptr<TChain> ch(new TChain("test"));
  ch->Add((app.dir + "/*.root").c_str());
  BOOST_CHECK_NE(0, ch->GetEntries());
  TArray* arr = nullptr;
  std::shared_ptr<TArrayS> as(new TArrayS());
  std::shared_ptr<TArrayI> ai(new TArrayI());
  std::shared_ptr<TArrayL> al(new TArrayL());
  std::shared_ptr<TArrayF> af(new TArrayF());
  std::shared_ptr<TArrayD> ad(new TArrayD());
  std::shared_ptr<TArrayC> ac(new TArrayC());
  std::shared_ptr<ChimeraTK::detail::TArrayStr> astr(new ChimeraTK::detail::TArrayStr());
  if(t == 0) {
    arr = as.get();
    auto p = as.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 1) {
    arr = ai.get();
    auto p = ai.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 2) {
    arr = al.get();
    auto p = al.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 3) {
    arr = af.get();
    auto p = af.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 4) {
    arr = ad.get();
    auto p = ad.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 5) {
    arr = ac.get();
    auto p = ac.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  else if(t == 6) {
    arr = astr.get();
    auto p = astr.get();
    ch->SetBranchAddress("Dummy.out", &p);
  }
  ch->GetEvent(1);
  // array is 1,3,5,7,9
  BOOST_CHECK_EQUAL(arr->GetSize(), 5);
  if constexpr(std::is_same<T, bool>::value) {
    for(size_t i = 0; i < 5; i++) {
      BOOST_CHECK_EQUAL(arr->GetAt(i), false);
    }
  }
  else {
    for(size_t i = 0; i < 5; i++) {
      BOOST_CHECK_EQUAL(arr->GetAt(i), 2 * i + 1);
    }
  }
  // remove currentBuffer and data0000.root to data0004.root and the directory uDAQ
  BOOST_CHECK_EQUAL(boost::filesystem::remove_all(app.dir), 7);
}
