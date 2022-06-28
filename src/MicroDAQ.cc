/*
 * MicroDAQ.cc
 *
 *  Created on: 22.03.2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "MicroDAQ.h"

#include <fstream>
#include <iostream>
#include <string.h>

#include <boost/format.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/algorithm/string.hpp>

#include <ChimeraTK/ApplicationCore/ControlSystemModule.h>
#include <ChimeraTK/ApplicationCore/DeviceModule.h>

#ifdef ENABLE_HDF5
#  include "MicroDAQHDF5.h"
#endif
#ifdef ENABLE_ROOT
#  include "MicroDAQROOT.h"
#endif

namespace ChimeraTK {

  template<typename TRIGGERTYPE>
  MicroDAQ<TRIGGERTYPE>::MicroDAQ(EntityOwner* owner, const std::string& name, const std::string& description,
      const std::string& inputTag, const std::string& pathToTrigger, HierarchyModifier hierarchyModifier,
      const std::unordered_set<std::string>& tags)
  : ModuleGroup(owner, name, "", HierarchyModifier::hideThis) {
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
          this, name, description, decimationFactor, decimationThreshold, hierarchyModifier, tags, pathToTrigger);
#else
      throw ChimeraTK::logic_error("MicroDAQ: Output format HDF5 selected but not compiled in.");
#endif
    }
    else if(type == "root") {
#ifdef ENABLE_ROOT
      impl = std::make_shared<RootDAQ<TRIGGERTYPE>>(
          this, name, description, decimationFactor, decimationThreshold, hierarchyModifier, tags, pathToTrigger);
#else
      throw ChimeraTK::logic_error("MicroDAQ: Output format ROOT selected but not compiled in.");
#endif
    }
    else {
      throw ChimeraTK::logic_error("MicroDAQ: Unknown output format specified in config file: '" + type + "'.");
    }

    // connect input data with the DAQ implementation
    impl->addSource(owner->findTag(inputTag));
  }

  template<typename TRIGGERTYPE>
  void MicroDAQ<TRIGGERTYPE>::addDeviceModule(
      const DeviceModule& source, const RegisterPath& namePrefix, const std::string& submodule) {
    if(impl) {
      auto mod = source.virtualiseFromCatalog();
      if(submodule.empty()) {
        impl->addSource(mod, namePrefix);
      }
      else {
        impl->addSource(mod.submodule(submodule), namePrefix);
      }
    }
  }

  namespace detail {

    /** Callable class for use with  boost::fusion::for_each: Attach the given
     * accessor to the DAQ with proper handling of the UserType. */
    template<typename TRIGGERTYPE>
    struct BaseDAQAccessorAttacher {
      BaseDAQAccessorAttacher(VariableNetworkNode& feeder, BaseDAQ<TRIGGERTYPE>* owner, const std::string& name)
      : _feeder(feeder), _owner(owner), _name(name) {}

      template<typename PAIR>
      void operator()(PAIR&) const {
        // only continue if the call is for the right type
        if(typeid(typename PAIR::first_type) != _feeder.getValueType()) return;

        // register connection
        try {
          if(_feeder.getMode() == UpdateMode::poll && _feeder.getDirection().dir == VariableDirection::feeding)
            _feeder[_owner->triggerGroup.trigger] >> _owner->template getAccessor<typename PAIR::first_type>(_name);
          else
            _feeder >> _owner->template getAccessor<typename PAIR::first_type>(_name);
        }
        catch(ChimeraTK::logic_error& e) {
          std::cout << "Failed to add accessor with name: " << _name << ", because it is already registered for DAQ."
                    << std::endl;
          return;
        }
      }

      VariableNetworkNode& _feeder;
      BaseDAQ<TRIGGERTYPE>* _owner;
      const std::string& _name;
    };

  } // namespace detail

  template<typename TRIGGERTYPE>
  template<typename UserType>
  VariableNetworkNode BaseDAQ<TRIGGERTYPE>::getAccessor(const std::string& variableName) {
    // check if variable name already registered
    for(auto& name : _overallVariableList) {
      if(name == variableName) {
        throw ChimeraTK::logic_error("Cannot add '" + variableName +
            "' to MicroDAQ since a variable with that "
            "name is already registered.");
      }
    }
    _overallVariableList.push_back(variableName);

    // add accessor and name to lists
    auto& tmpAccessorList = boost::fusion::at_key<UserType>(_accessorListMap.table);
    auto& nameList = boost::fusion::at_key<UserType>(_nameListMap.table);
    auto dirName = variableName.substr(0, variableName.find_last_of("/"));
    auto baseName = variableName.substr(variableName.find_last_of("/") + 1);
    if(BaseDAQ<TRIGGERTYPE>::_groupMap.count(dirName) < 1) {
      tmpAccessorList.emplace_back(this, baseName, "", 0, "");
    }
    else {
      tmpAccessorList.emplace_back(&BaseDAQ<TRIGGERTYPE>::_groupMap[dirName], baseName, "", 0, "");
    }
    nameList.push_back(variableName);

    // add internal tag so we can exclude these variables in findTagAndAppendToModule()
    tmpAccessorList.back().addTag("***MicroDAQ-internal***");

    // return the accessor
    return tmpAccessorList.back();
  }

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::addSource(const Module& source, const RegisterPath& namePrefix, const bool& isCSModule) {
    // for simplification, first create a VirtualModule containing the correct
    // hierarchy structure (obeying eliminate hierarchy etc.)
    auto dynamicModel = source.findTag(".*"); /// @todo use virtualise() instead

    // create variable group map for namePrefix if needed
    if(_groupMap.find(namePrefix) == _groupMap.end()) {
      // search for existing parent (if any)
      auto parentPrefix = namePrefix;
      while(_groupMap.find(parentPrefix) == _groupMap.end()) {
        if(parentPrefix == "/") break; // no existing parent found
        parentPrefix = std::string(parentPrefix).substr(0, std::string(parentPrefix).find_last_of("/"));
      }
      // create all not-yet-existing parents
      while(parentPrefix != namePrefix) {
        EntityOwner* owner = this;
        if(parentPrefix != "/") owner = &_groupMap[parentPrefix];
        auto stop = std::string(namePrefix).find_first_of("/", parentPrefix.length() + 1);
        if(stop == std::string::npos) stop = namePrefix.length();
        RegisterPath name = std::string(namePrefix).substr(parentPrefix.length(), stop - parentPrefix.length());
        parentPrefix /= name;
        _groupMap[parentPrefix] = VariableGroup(owner, std::string(name).substr(1), "");
      }
    }

    // add all accessors on this hierarchy level
    for(auto& acc : dynamicModel.getAccessorList()) {
      // seems like getName returns the full path for ControlSystemModule whereas it returns only the accessor name for ApplicationModule
      // Btw getQualifiedName returns the full path for ApplicationModule and nothing for ControlsystemModule
      std::string name = acc.getName();
      // for modules with deleted hierarchy names might still include '/',
      // e.g. Controller includes 'FeedForward/Table/I' and remove FeedForward/Table would lead to multiple
      // I variables with namePrefix Controller -> only do the removal for CS modules
      if(isCSModule && name.find("/") != std::string::npos) name = name.substr(name.find_last_of("/") + 1);
      boost::fusion::for_each(
          _accessorListMap.table, detail::BaseDAQAccessorAttacher<TRIGGERTYPE>(acc, this, namePrefix / name));
    }

    // recurse into submodules
    for(auto mod : dynamicModel.getSubmoduleList()) {
      addSource(*mod, namePrefix / mod->getName(), isCSModule);
    }
  }

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::addSource(const Module& source, const RegisterPath& namePrefix) {
    //\ToDo: Rework the variable name creation without CS specific workaround
    bool isCSModule = false;
    if(dynamic_cast<const ControlSystemModule*>(&source) != nullptr) isCSModule = true;
    addSource(source, namePrefix, isCSModule);
  }

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
      currentPath = _daqPath.string();
      currentPath.write();
    }
    catch(...) {
      std::cerr << "Failed setting new DAQ path." << std::endl;
    }
  }
  template<typename TRIGGERTYPE>
  bool BaseDAQ<TRIGGERTYPE>::checkFile() {
    try {
      if(boost::filesystem::exists(_daqPath)) {         // does p actually exist?
        if(boost::filesystem::is_directory(_daqPath)) { // is p a directory?
          for(auto i = boost::filesystem::directory_iterator(_daqPath); i != boost::filesystem::directory_iterator();
              i++) {
            std::string match = (boost::format("buffer%04d%s") % currentBuffer % _suffix).str();
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

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::deleteRingBufferFile() {
    try {
      if(boost::filesystem::exists(_daqPath)) {
        if(boost::filesystem::is_directory(_daqPath)) {
          for(auto i = boost::filesystem::directory_iterator(_daqPath); i != boost::filesystem::directory_iterator();
              i++) {
            std::string match = (boost::format("buffer%04d%s") % currentBuffer % _suffix).str();
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

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::checkBufferOnFirstTrigger() {
    setDAQPath();
    try {
      // only create path if it is the default one
      if(((std::string)currentPath).compare(_daqDefaultPath) == 0) boost::filesystem::create_directory(_daqPath);
    }
    catch(boost::filesystem::filesystem_error& e) {
      std::cerr << "Failed to create DAQ directory: " << ((std::string)currentPath).c_str() << std::endl;
      std::cerr << e.what() << std::endl;
      return;
    }
    std::fstream bufferNumber;
    // determine current buffer number
    bufferNumber.open((_daqPath / "currentBuffer").c_str(), std::ofstream::in);
    bufferNumber.seekg(0);
    if(!bufferNumber.eof()) {
      bufferNumber >> currentBuffer;
      std::string filename = _prefix + (boost::format("_buffer%04d%s") % currentBuffer % _suffix).str();
      if(checkFile()) {
        currentBuffer++;
        currentBuffer.write();
      }
      if(currentBuffer >= nMaxFiles) {
        currentBuffer = 0;
        currentBuffer.write();
      }
    }
    else {
      currentBuffer = 0;
      currentBuffer.write();
    }
    bufferNumber.close();
  }

  template<typename TRIGGERTYPE>
  std::string BaseDAQ<TRIGGERTYPE>::nextBuffer() {
    std::vector<std::string> result;
    // read local time e.g. 20200512T134659.777223
    std::string timeStampStr(boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time()));
    // remove sub second part -> 20200512T134659
    boost::algorithm::split(result, timeStampStr, boost::is_any_of("."));
    _prefix = result.at(0);

    if(currentBuffer >= nMaxFiles) {
      currentBuffer = 0;
      currentBuffer.write();
    }
    std::fstream bufferNumber;
    std::string filename = _prefix + (boost::format("_buffer%04d%s") % currentBuffer % _suffix).str();
    // store current buffer number to disk
    bufferNumber.open((_daqPath / "currentBuffer").c_str(), std::ofstream::out);
    bufferNumber << currentBuffer << std::endl;
    bufferNumber.close();

    deleteRingBufferFile();

    return filename;
  }

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::updateDAQPath() {
    if(enable == 0) {
      if(((std::string)setPath).empty()) {
        if(((std::string)currentPath).compare(_daqDefaultPath) != 0) setDAQPath();
      }
      else if(((std::string)setPath).compare((std::string)currentPath) != 0) {
        setDAQPath();
      }
    }
  }

  template<typename TRIGGERTYPE>
  bool BaseDAQ<TRIGGERTYPE>::maxEntriesReached() {
    if(currentEntry == nTriggersPerFile) {
      // increase current buffer number, nextBuffer will check buffer size
      currentBuffer++;
      currentBuffer.write();
      currentEntry = 0;
      currentEntry.write();
      return true;
    }
    return false;
  }

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::disableDAQ() {
    currentEntry = 0;
    currentEntry.write();
    errorStatus = 0;
    errorStatus.write();
    // increase current buffer number, nextBuffer will check buffer size
    currentBuffer++;
    currentBuffer.write();
  }

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::findTagAndAppendToModule(
      VirtualModule& virtualParent, const std::string& tag, bool, bool, bool negate, VirtualModule& root) const {
    // Change behaviour to exclude the auto-generated inputs which are connected to the data sources. Otherwise those
    // variables might get published twice to the control system, if findTag(".*") is used to connect the entire
    // application to the control system.
    // This is a temporary solution. In future, instead the inputs should be generated at the same place in the
    // hierarchy as the source variable, and the connection should not be made by the module itself. It will be rather
    // expected from the application to connect everything to a ControlSystemModule. addSource() should then also be
    // removed, and the variable household of the application should instead be scanned for variables matching the
    // given tag (see MicroDAQ envelope class). This currently does not yet work because of a missing concept:
    // device variables currently do not know tags and hence it would not be possible to selectively add some device
    // variables to the DAQ without adding the entire application.

    struct MyVirtualModule : VirtualModule {
      using VirtualModule::VirtualModule;
      using VirtualModule::submodules;
    };

    MyVirtualModule temporary("temporary", "", ModuleType::ApplicationModule);
    EntityOwner::findTagAndAppendToModule(temporary, R"(\*\*\*MicroDAQ-internal\*\*\*)", false, false, true, temporary);
    for(auto& sm : temporary.submodules) {
      sm.findTagAndAppendToModule(virtualParent, tag, false, false, negate, root);
    }
  }

  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES_NO_VOID(MicroDAQ);
  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES_NO_VOID(BaseDAQ);
} // namespace ChimeraTK
