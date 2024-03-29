// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * MicroDAQROOT.cc
 *
 *  Created on: Feb 3, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "MicroDAQROOT.h"

#include "data_types.h"
#include "TFile.h"
#include "TTimeStamp.h"
#include "TTree.h"

#include <list>

namespace ChimeraTK {

  namespace detail {

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    struct ROOTstorage {
      ROOTstorage(RootDAQ<TRIGGERTYPE>* owner) : outFile(nullptr), tree(nullptr), _owner(owner) {
        missedTrigger.parameter["missedTrigger"];
      }
      ~ROOTstorage() { close(); }

      void close() {
        if(tree && outFile) {
          if(!tree->Write()) {
            std::cerr << "No data written to file, when writing the TTree." << std::endl;
          }
          outFile->Close();
          outFile = nullptr;
          tree = nullptr;
        }
      }

      TFile* outFile;
      TTree* tree;
      std::string currentGroupName;

      /** Unique list of groups, used to create the groups in the file */
      std::list<std::string> groupList;

      /** boost::fusion::map of UserTypes to std::lists containing decimation
       * factors. */
      template<typename UserType>
      using decimationFactorList = std::list<size_t>;
      TemplateUserTypeMapNoVoid<decimationFactorList> decimationFactorListMap;

      template<typename UserType>
      using fieldData = TreeDataFields<UserType>;
      TemplateUserTypeMapNoVoid<fieldData> treeDataMap;

      TTimeStamp timeStamp;

      TreeDataFields<TRIGGERTYPE> missedTrigger{};
      Long64_t triggerPeriod{};

      bool firstTrigger{true};

      void processTrigger();

      RootDAQ<TRIGGERTYPE>* _owner;

      /**
       *  Collect all accessors that use the DAQ trigger as external trigger.
       *  This does not really belong to storage but since we iterate over all accessors here
       *  we include that step here.
       */
      std::vector<TransferElementID> _accessorsWithTrigger;
    };

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    struct ROOTDataSpaceCreator {
      ROOTDataSpaceCreator(ROOTstorage<TRIGGERTYPE>& storage) : _storage(storage) {}

      template<typename PAIR>
      void operator()(PAIR& pair) const {
        typedef typename PAIR::first_type UserType;

        // get the lists for the UserType
        auto& accessorList = pair.second;
        auto& decimationFactorList = boost::fusion::at_key<UserType>(_storage.decimationFactorListMap.table);
        auto& treeData = boost::fusion::at_key<UserType>(_storage.treeDataMap.table);
        auto& nameList = boost::fusion::at_key<UserType>(_storage._owner->_nameListMap.table);
        auto& branchList = boost::fusion::at_key<UserType>(_storage._owner->_branchNameList.table);

        // iterate through all accessors for this UserType
        auto name = nameList.begin();
        for(auto accessor = accessorList.begin(); accessor != accessorList.end(); ++accessor, ++name) {
          // check if accessor uses DAQ trigger as external trigger
          if(_storage._owner->isAccessorUsingDAQTrigger(*accessor)) {
            _storage._accessorsWithTrigger.push_back(accessor->getId());
          }

          // determine decimation factor
          int factor = 1;
          if(accessor->getNElements() > _storage._owner->_decimationThreshold) {
            factor = _storage._owner->_decimationFactor;
          }
          decimationFactorList.push_back(factor);

          /* Format the names -> replace '/' with '.'
           * This format is used for ROOT branch names
           */
          std::string nameWithDot = *name;
          replace(nameWithDot.begin(), nameWithDot.end(), '/', '.');
          // remove leading '.'
          if(nameWithDot.at(0) != '.') throw ChimeraTK::logic_error("Unexpected register name.");
          nameWithDot = nameWithDot.substr(1, nameWithDot.length());
          branchList.push_back(nameWithDot);
          // Add map entry -> based on the length create a scalar or an array
          if(accessor->getNElements() > 1) {
            // create map entry (empty array)
            auto array = treeData.trace[nameWithDot];
            // set array length
            array.Set(accessor->getNElements() / factor);
            // assign array with correct length to the map entry
            treeData.trace[nameWithDot] = array;
          }
          else {
            treeData.parameter[nameWithDot];
          }
          // put all group names in list (each hierarchy level separately)
          size_t idx = 0;
          while((idx = name->find('/', idx + 1)) != std::string::npos) {
            std::string groupName = name->substr(0, idx);
            _storage.groupList.push_back(groupName);
          }
        }
      }

      ROOTstorage<TRIGGERTYPE>& _storage;
    };

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    struct ROOTTreeCreator {
      ROOTTreeCreator(ROOTstorage<TRIGGERTYPE>& storage, const std::string& name) : _storage(storage), _name(name) {}

      template<typename PAIR>
      void operator()(PAIR&) const {
        typedef typename PAIR::first_type UserType;

        if(!_storage.tree) _storage.tree = new TTree(_name.c_str(), "Data produced by ChimeraTK RootDAQ module");
        // get the lists for the UserType
        auto& treeData = boost::fusion::at_key<UserType>(_storage.treeDataMap.table);

        for(auto& accessor : treeData.parameter) {
          // determine decimation factor
          if(_storage.tree->Branch(accessor.first.c_str(), &accessor.second) == nullptr) {
            throw ChimeraTK::logic_error((std::string("Failed to add branch for variable ") + accessor.first).c_str());
          }
        }
        for(auto& accessor : treeData.trace) {
          // determine decimation factor
          if(_storage.tree->Branch(accessor.first.c_str(), &accessor.second) == nullptr) {
            throw ChimeraTK::logic_error((std::string("Failed to add branch for variable ") + accessor.first).c_str());
          }
        }
      }

      ROOTstorage<TRIGGERTYPE>& _storage;
      std::string _name;
    };

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    struct ROOTDataWriter {
      ROOTDataWriter(ROOTstorage<TRIGGERTYPE>& storage) : _storage(storage) {}

      template<typename PAIR>
      void operator()(PAIR&) const {
        typedef typename PAIR::first_type UserType;

        // get the lists for the UserType
        auto& decimationFactorList = boost::fusion::at_key<UserType>(_storage.decimationFactorListMap.table);
        auto& accessorList = boost::fusion::at_key<UserType>(_storage._owner->_accessorListMap.table);
        auto& branchList = boost::fusion::at_key<UserType>(_storage._owner->_branchNameList.table);
        auto& treeDataMap = boost::fusion::at_key<UserType>(_storage.treeDataMap.table);

        auto branchName = branchList.begin();
        auto decimationFactor = decimationFactorList.begin();

        for(auto accessor = accessorList.begin(); accessor != accessorList.end();
            ++accessor, ++branchName, ++decimationFactor) {
          if(accessor->getNElements() > 1) {
            size_t n = accessor->getNElements() / (*decimationFactor);
            for(size_t i = 0; i < n; i++) treeDataMap.trace[*branchName][i] = (*accessor)[i * (*decimationFactor)];
          }
          else {
            treeDataMap.parameter[*branchName] = (*accessor)[0];
          }
        }
      }

      ROOTstorage<TRIGGERTYPE>& _storage;
    };

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    void ROOTstorage<TRIGGERTYPE>::processTrigger() {
      // update daqPath if DAQ is disabled
      _owner->updateDAQPath();

      // need to open or close file?
      if(!outFile && _owner->enable != 0 && _owner->status.errorStatus == 0) {
        // some things to be done only on first trigger
        if(firstTrigger) {
          _owner->checkBufferOnFirstTrigger();
          firstTrigger = false;
        }

        std::string filename = _owner->nextBuffer();
        // open file
        outFile = TFile::Open((_owner->_daqPath / filename).c_str(), "RECREATE");
        if(outFile) {
          outFile->SetCompressionAlgorithm(ROOT::RCompressionSetting::EAlgorithm::kZSTD);
          outFile->SetCompressionLevel(ROOT::RCompressionSetting::ELevel::kDefaultZSTD);
        }
      }
      // close file and update error status for inactive DAQ
      else if(outFile && _owner->enable == 0) {
        close();
        _owner->disableDAQ();
      }

      if(outFile) {
        if(!tree) {
          boost::fusion::for_each(treeDataMap.table, ROOTTreeCreator<TRIGGERTYPE>(*this, _owner->_treeName));
          tree->Branch("MicroDAQ.triggerPeriod", &triggerPeriod);
          tree->Branch("MicroDAQ.nMissedTriggers", &missedTrigger.parameter["missedTrigger"]);
          tree->Branch("timeStamp", &timeStamp);
        }
        // construct time stamp
        timeStamp = TTimeStamp();

        // write data
        boost::fusion::for_each(treeDataMap.table, ROOTDataWriter<TRIGGERTYPE>(*this));
        missedTrigger.parameter["missedTrigger"] = _owner->BaseDAQ<TRIGGERTYPE>::status.nMissedTriggers;
        triggerPeriod = _owner->BaseDAQ<TRIGGERTYPE>::status.triggerPeriod;
        tree->Fill();
        _owner->status.currentEntry = _owner->status.currentEntry + 1;
        _owner->status.currentEntry.write();
      }

      // update error status for active DAQ
      if(_owner->enable == 1) {
        // only write error message once
        if(!outFile && _owner->status.errorStatus == 0) {
          std::cerr
              << "Something went wrong. File could not be opened. Solve the problem and toggle enable DAQ to try again."
              << std::endl;
          _owner->status.errorStatus = 1;
          _owner->status.errorStatus.write();
          delete outFile;
        }
        else if(outFile && _owner->status.errorStatus != 0) {
          _owner->status.errorStatus = 0;
          _owner->status.errorStatus.write();
        }
      }
      // close file if all triggers are filled
      if(outFile) {
        auto nEntries = tree->GetEntriesFast();
        if(_owner->flushAfterNEntries > 0 && nEntries % _owner->flushAfterNEntries == 1) tree->AutoSave("SaveSelf");
        if(_owner->maxEntriesReached()) close();
      }
    }

  } // namespace detail

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void RootDAQ<TRIGGERTYPE>::mainLoop() {
    std::cout << "Initializing ROOTDAQ system..." << std::endl;

    // storage object
    detail::ROOTstorage<TRIGGERTYPE> storage(this);

    // create the data spaces and look for accessors using the DAQ trigger as external node
    boost::fusion::for_each(
        BaseDAQ<TRIGGERTYPE>::_accessorListMap.table, detail::ROOTDataSpaceCreator<TRIGGERTYPE>(storage));

    // add trigger
    storage._accessorsWithTrigger.push_back(BaseDAQ<TRIGGERTYPE>::trigger.getId());

    // sort group list and make unique to make sure lower levels get created first
    storage.groupList.sort();
    storage.groupList.unique();

    // write initial values
    storage.processTrigger();

    // loop: process incoming triggers
    auto group = ApplicationModule::readAnyGroup();
    while(true) {
      // Wait for the DAQ trigger and an update of all accessors using the DAQ trigger as external node
      group.readUntilAll(storage._accessorsWithTrigger);
      storage.processTrigger();
      BaseDAQ<TRIGGERTYPE>::updateDiagnostics();
    }
  }

  /********************************************************************************************************************/

  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES_NO_VOID(RootDAQ);

  /********************************************************************************************************************/

} // namespace ChimeraTK
