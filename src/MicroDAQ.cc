// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * MicroDAQ.cc
 *
 *  Created on: 22.03.2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "MicroDAQ.h"

#include <ChimeraTK/ApplicationCore/DeviceModule.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>

#include <fstream>
#include <iostream>
#include <string.h>
#include <vector>

#ifdef ENABLE_HDF5
#  include "MicroDAQHDF5.h"
#endif
#ifdef ENABLE_ROOT
#  include "MicroDAQROOT.h"
#endif

namespace ChimeraTK {

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  MicroDAQ<TRIGGERTYPE>::MicroDAQ(ModuleGroup* owner, const std::string& name, const std::string& description,
      const std::string& inputTag, const std::string& pathToTrigger, const std::unordered_set<std::string>& tags)
  : ModuleGroup(owner, ".", "") {
    // do nothing if the entire module is disabled
    if(appConfig().template get<Boolean>("Configuration/MicroDAQ/enable") == false) return;

    // obtain desired output format from configuration and convert to lower case
    auto type = appConfig().template get<std::string>("Configuration/MicroDAQ/outputFormat");
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return std::tolower(c); });

    // obtain decimation factor from configuration
    uint32_t decimationFactor = appConfig().template get<uint32_t>("Configuration/MicroDAQ/decimationFactor");
    uint32_t decimationThreshold = appConfig().template get<uint32_t>("Configuration/MicroDAQ/decimationThreshold");

    // instantiate DAQ implementation for the desired output format
    if(type == "hdf5") {
#ifdef ENABLE_HDF5
      impl = std::make_shared<HDF5DAQ<TRIGGERTYPE>>(
          this, name, description, decimationFactor, decimationThreshold, tags, pathToTrigger);
#else
      throw ChimeraTK::logic_error("MicroDAQ: Output format HDF5 selected but not compiled in.");
#endif
    }
    else if(type == "root") {
#ifdef ENABLE_ROOT
      impl = std::make_shared<RootDAQ<TRIGGERTYPE>>(
          this, name, description, decimationFactor, decimationThreshold, tags, pathToTrigger);
#else
      throw ChimeraTK::logic_error("MicroDAQ: Output format ROOT selected but not compiled in.");
#endif
    }
    else {
      throw ChimeraTK::logic_error("MicroDAQ: Unknown output format specified in config file: '" + type + "'.");
    }

    // connect input data with the DAQ implementation
    impl->addSource(".", inputTag);
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void MicroDAQ<TRIGGERTYPE>::addDeviceModule(
      DeviceModule& source, const RegisterPath& namePrefix, const RegisterPath& submodule) {
    if(impl) {
      std::vector<Model::ProcessVariableProxy> pvs;
      source.getModel().visit([&](auto pv) { pvs.emplace_back(pv); }, Model::keepPvAccess, Model::adjacentSearch,
          Model::keepProcessVariables);
      for(auto pv : pvs) {
        impl->addVariableFromModel(pv, namePrefix, submodule);
      }
    }
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::addSource(const std::string& qualifiedDirectoryPath, const std::string& inputTag) {
    auto model = dynamic_cast<ModuleGroup*>(_owner)->getModel();
    auto neighbourDir = model.visit(ChimeraTK::Model::returnDirectory, ChimeraTK::Model::getNeighbourDirectory,
        ChimeraTK::Model::returnFirstHit(ChimeraTK::Model::DirectoryProxy{}));

    std::vector<ChimeraTK::Model::ProcessVariableProxy> pvs;
    auto found = neighbourDir.visitByPath(qualifiedDirectoryPath, [&](auto sourceDir) {
      if(inputTag.empty()) {
        sourceDir.visit([&](auto pv) { pvs.emplace_back(pv); }, ChimeraTK::Model::breadthFirstSearch,
            ChimeraTK::Model::keepProcessVariables);
      }
      else {
        sourceDir.visit([&](auto pv) { pvs.emplace_back(pv); }, ChimeraTK::Model::breadthFirstSearch,
            ChimeraTK::Model::keepProcessVariables && ChimeraTK::Model::keepTag(inputTag));
      }
    });

    for(auto pv : pvs) {
      addVariableFromModel(pv);
    }

    if(!found) {
      throw ChimeraTK::logic_error("Path passed to BaseDAQ<TRIGGERTYPE>::addSource() not found!");
    }
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::setDAQPath() {
    try {
      if(((std::string)setPath).empty()) {
        _daqPath = boost::filesystem::current_path() / "uDAQ";
      }
      else {
        _daqPath = boost::filesystem::path((std::string)setPath);
      }
      std::cout << "Set new DAQ path: " << _daqPath.string().c_str() << std::endl;
      status.currentPath = _daqPath.string();
      status.currentPath.write();
    }
    catch(...) {
      std::cerr << "Failed setting new DAQ path." << std::endl;
    }
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  bool BaseDAQ<TRIGGERTYPE>::checkFile() {
    try {
      if(boost::filesystem::exists(_daqPath)) {         // does p actually exist?
        if(boost::filesystem::is_directory(_daqPath)) { // is p a directory?
          for(auto i = boost::filesystem::directory_iterator(_daqPath); i != boost::filesystem::directory_iterator();
              i++) {
            std::string match = (boost::format("buffer%04d%s") % status.currentBuffer % _suffix).str();
            if(boost::filesystem::canonical(i->path()).string().find(match) != std::string::npos) {
              return boost::filesystem::file_size(i->path()) > 1000;
            }
          }
        }
      }
    }
    catch(const boost::filesystem::filesystem_error& ex) {
      std::cout << "Checking existing buffer file failed:" << std::endl;
      std::cout << ex.what() << std::endl;
    }
    return false;
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::deleteRingBufferFile() {
    try {
      if(boost::filesystem::exists(_daqPath)) {
        if(boost::filesystem::is_directory(_daqPath)) {
          for(auto i = boost::filesystem::directory_iterator(_daqPath); i != boost::filesystem::directory_iterator();
              i++) {
            std::string match = (boost::format("buffer%04d%s") % status.currentBuffer % _suffix).str();
            if(boost::filesystem::canonical(i->path()).string().find(match) != std::string::npos) {
              boost::filesystem::remove(i->path());
            }
          }
        }
      }
    }
    catch(const boost::filesystem::filesystem_error& ex) {
      std::cerr << "Ringbuffer file delete failed:" << std::endl;
      std::cout << ex.what() << std::endl;
    }
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::checkBufferOnFirstTrigger() {
    setDAQPath();
    try {
      // only create path if it is the default one
      if(((std::string)status.currentPath).compare(_daqDefaultPath) == 0) boost::filesystem::create_directory(_daqPath);
    }
    catch(boost::filesystem::filesystem_error& e) {
      std::cerr << "Failed to create DAQ directory: " << ((std::string)status.currentPath).c_str() << std::endl;
      std::cerr << e.what() << std::endl;
      return;
    }
    std::fstream bufferNumber;
    // determine current buffer number
    bufferNumber.open((_daqPath / "currentBuffer").c_str(), std::ofstream::in);
    bufferNumber.seekg(0);
    if(!bufferNumber.eof()) {
      bufferNumber >> status.currentBuffer;
      std::string filename = _prefix + (boost::format("_buffer%04d%s") % status.currentBuffer % _suffix).str();
      if(checkFile()) {
        status.currentBuffer++;
        status.currentBuffer.write();
      }
      if(status.currentBuffer >= nMaxFiles) {
        status.currentBuffer = 0;
        status.currentBuffer.write();
      }
    }
    else {
      status.currentBuffer = 0;
      status.currentBuffer.write();
    }
    bufferNumber.close();
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  std::string BaseDAQ<TRIGGERTYPE>::nextBuffer() {
    std::vector<std::string> result;
    // read local time e.g. 20200512T134659.777223
    std::string timeStampStr(boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time()));
    // remove sub second part -> 20200512T134659
    boost::algorithm::split(result, timeStampStr, boost::is_any_of("."));
    _prefix = result.at(0);

    if(status.currentBuffer >= nMaxFiles) {
      status.currentBuffer = 0;
      status.currentBuffer.write();
    }
    std::fstream bufferNumber;
    std::string filename = _prefix + (boost::format("_buffer%04d%s") % status.currentBuffer % _suffix).str();
    // store current buffer number to disk
    bufferNumber.open((_daqPath / "currentBuffer").c_str(), std::ofstream::out);
    bufferNumber << status.currentBuffer << std::endl;
    bufferNumber.close();

    deleteRingBufferFile();

    return filename;
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::updateDAQPath() {
    if(enable == 0) {
      if(((std::string)setPath).empty()) {
        if(((std::string)status.currentPath).compare(_daqDefaultPath) != 0) setDAQPath();
      }
      else if(((std::string)setPath).compare((std::string)status.currentPath) != 0) {
        setDAQPath();
      }
    }
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  bool BaseDAQ<TRIGGERTYPE>::maxEntriesReached() {
    if(status.currentEntry == nTriggersPerFile) {
      // increase current buffer number, nextBuffer will check buffer size
      status.currentBuffer++;
      status.currentBuffer.write();
      status.currentEntry = 0;
      status.currentEntry.write();
      return true;
    }
    return false;
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::disableDAQ() {
    status.currentEntry = 0;
    status.currentEntry.write();
    status.errorStatus = 0;
    status.errorStatus.write();
    // increase current buffer number, nextBuffer will check buffer size
    status.currentBuffer++;
    status.currentBuffer.write();
  }

  /********************************************************************************************************************/

  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES_NO_VOID(MicroDAQ);
  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES_NO_VOID(BaseDAQ);

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::prepare() {
    if(!_overallVariableList.size()) {
      throw logic_error(
          "No variables are connected to the MicroDAQ module. Did you use the correct tag or connect a Device?");
    }
  }

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::updateDiagnostics() {
    if constexpr(std::is_integral_v<TRIGGERTYPE>) {
      status.nMissedTriggers = (TRIGGERTYPE)trigger - lastTrigger - 1;
      lastTrigger = (TRIGGERTYPE)trigger;
      status.nMissedTriggers.write();
    }
    if(lastVersion != VersionNumber{}) {
      std::chrono::duration<double> diff = trigger.getVersionNumber().getTime() - lastVersion.getTime();
      std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
      status.triggerPeriod = ms.count();
      status.triggerPeriod.write();
    }
    lastVersion = BaseDAQ<TRIGGERTYPE>::trigger.getVersionNumber();
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
