/*
 * MicroDAQHDF5.cc
 *
 *  Created on: 21.03.2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "MicroDAQHDF5.h"

#include <H5Cpp.h>
#include <map>

namespace ChimeraTK {
  namespace detail {

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    struct H5storage {
      H5storage(HDF5DAQ<TRIGGERTYPE>* owner) : _owner(owner) {
        // prepare internal data
        hsize_t dimsf[1] = {1}; // dataset dimensions
        _space["MicroDAQ.nMissedTriggers"] = H5::DataSpace(1, dimsf);
        _space["MicroDAQ.triggerPeriod"] = H5::DataSpace(1, dimsf);
      }

      std::unique_ptr<H5::H5File> outFile{};
      std::string currentGroupName;

      /** Unique list of groups, used to create the groups in the file */
      std::list<std::string> groupList;

      /** boost::fusion::map of UserTypes to std::lists containing the H5::DataSpace
       * objects. */
      template<typename UserType>
      using dataSpaceList = std::list<H5::DataSpace>;
      TemplateUserTypeMap<dataSpaceList> dataSpaceListMap;

      /** boost::fusion::map of UserTypes to std::lists containing decimation
       * factors. */
      template<typename UserType>
      using decimationFactorList = std::list<size_t>;
      TemplateUserTypeMap<decimationFactorList> decimationFactorListMap;

      bool isOpened{false};
      bool firstTrigger{true};

      void processTrigger();
      void writeData();

      HDF5DAQ<TRIGGERTYPE>* _owner;

      /**
       *  Collect all accessors that use the DAQ trigger as external trigger.
       *  This does not really belong to storage but since we iterate over all accessors here
       *  we include that step here.
       */
      std::vector<TransferElementID> _accessorsWithTrigger;

     private:
      std::vector<float> _buffer{1};
      std::map<std::string, H5::DataSpace> _space;
    };

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    struct H5DataSpaceCreator {
      H5DataSpaceCreator(H5storage<TRIGGERTYPE>& storage) : _storage(storage) {}

      template<typename PAIR>
      void operator()(PAIR& pair) const {
        typedef typename PAIR::first_type UserType;

        // get the lists for the UserType
        auto& accessorList = pair.second;
        auto& decimationFactorList = boost::fusion::at_key<UserType>(_storage.decimationFactorListMap.table);
        auto& dataSpaceList = boost::fusion::at_key<UserType>(_storage.dataSpaceListMap.table);
        auto& nameList = boost::fusion::at_key<UserType>(_storage._owner->_nameListMap.table);

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

          // define data space
          hsize_t dimsf[1]; // dataset dimensions
          dimsf[0] = accessor->getNElements() / factor;
          dataSpaceList.push_back(H5::DataSpace(1, dimsf));

          // put all group names in list (each hierarchy level separately)
          size_t idx = 0;
          while((idx = name->find('/', idx + 1)) != std::string::npos) {
            std::string groupName = name->substr(0, idx);
            _storage.groupList.push_back(groupName);
          }
        }
      }

      H5storage<TRIGGERTYPE>& _storage;
    };

  } // namespace detail

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void HDF5DAQ<TRIGGERTYPE>::mainLoop() {
    std::cout << "Initialising HDF5DAQ system..." << std::endl;

    // storage object
    detail::H5storage<TRIGGERTYPE> storage(this);

    // create the data spaces
    boost::fusion::for_each(
        BaseDAQ<TRIGGERTYPE>::_accessorListMap.table, detail::H5DataSpaceCreator<TRIGGERTYPE>(storage));

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

  namespace detail {

    template<typename TRIGGERTYPE>
    void H5storage<TRIGGERTYPE>::processTrigger() {
      // set new daqPath if DAQ is disabled
      _owner->updateDAQPath();

      // need to open or close file?
      if(!isOpened && _owner->enable != 0 && _owner->status.errorStatus == 0) {
        // some things to be done only on first trigger
        if(firstTrigger) {
          _owner->checkBufferOnFirstTrigger();
          firstTrigger = false;
        }

        std::string filename = _owner->nextBuffer();

        // open file
        try {
          outFile.reset(new H5::H5File{(_owner->_daqPath / filename).c_str(), H5F_ACC_TRUNC});
        }
        catch(H5::FileIException&) {
          return;
        }
        isOpened = true;
      }
      else if(isOpened && _owner->enable == 0) {
        outFile->close();
        isOpened = false;
        _owner->disableDAQ();
      }

      // if file is opened, this trigger should be included in the DAQ
      if(isOpened) {
        // write data
        writeData();
        _owner->status.currentEntry = _owner->status.currentEntry + 1;
        _owner->status.currentEntry.write();
      }

      // update error status for active DAQ
      if(_owner->enable == 1) {
        // only write error message once
        if(!isOpened && _owner->status.errorStatus == 0) {
          std::cerr
              << "Something went wrong. File could not be opened. Solve the problem and toggle enable DAQ to try again."
              << std::endl;
          _owner->status.errorStatus = 1;
          _owner->status.errorStatus.write();
        }
        else if(isOpened && _owner->status.errorStatus != 0) {
          _owner->status.errorStatus = 0;
          _owner->status.errorStatus.write();
        }
      }

      if(isOpened) {
        if(_owner->maxEntriesReached()) {
          // just close the file here, will re-open on next trigger
          outFile->close();
          isOpened = false;
        }
      }
    }

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    struct H5DataWriter {
      H5DataWriter(detail::H5storage<TRIGGERTYPE>& storage) : _storage(storage) {}

      template<typename PAIR>
      void operator()(PAIR& pair) const {
        typedef typename PAIR::first_type UserType;

        // get the lists for the UserType
        auto& accessorList = pair.second;
        auto& decimationFactorList = boost::fusion::at_key<UserType>(_storage.decimationFactorListMap.table);
        auto& dataSpaceList = boost::fusion::at_key<UserType>(_storage.dataSpaceListMap.table);
        auto& nameList = boost::fusion::at_key<UserType>(_storage._owner->_nameListMap.table);

        // iterate through all accessors for this UserType
        auto decimationFactor = decimationFactorList.begin();
        auto dataSpace = dataSpaceList.begin();
        auto name = nameList.begin();
        for(auto accessor = accessorList.begin(); accessor != accessorList.end();
            ++accessor, ++decimationFactor, ++dataSpace, ++name) {
          // form full path name of data set
          std::string dataSetName = _storage.currentGroupName + "/" + *name;

          // write to file (this is mainly a function call to allow template
          // specialisations at this point)
          try {
            write2hdf<UserType>(*accessor, dataSetName, *decimationFactor, *dataSpace);
          }
          catch(H5::FileIException&) {
            std::cout << "HDF5DAQ: ERROR writing data set " << dataSetName << std::endl;
            throw;
          }
        }
      }

      template<typename UserType>
      void write2hdf(ArrayPushInput<UserType>& accessor, std::string& name, size_t decimationFactor,
          H5::DataSpace& dataSpace) const;

      H5storage<TRIGGERTYPE>& _storage;
    };

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    template<typename UserType>
    void H5DataWriter<TRIGGERTYPE>::write2hdf(ArrayPushInput<UserType>& accessor, std::string& dataSetName,
        size_t decimationFactor, H5::DataSpace& dataSpace) const {
      // prepare decimated buffer
      size_t n = accessor.getNElements() / decimationFactor;
      std::vector<float> buffer(n);
      for(size_t i = 0; i < n; ++i) {
        buffer[i] = userTypeToNumeric<float>(accessor[i * decimationFactor]);
      }

      // write data from internal buffer to data set in HDF5 file
      H5::DataSet dataset{_storage.outFile->createDataSet(dataSetName, H5::PredType::NATIVE_FLOAT, dataSpace)};
      dataset.write(buffer.data(), H5::PredType::NATIVE_FLOAT);
    }

    /******************************************************************************************************************/

    template<typename TRIGGERTYPE>
    void H5storage<TRIGGERTYPE>::writeData() {
      // format current time
      struct timeval tv;
      gettimeofday(&tv, nullptr);
      time_t t = tv.tv_sec;
      if(t == 0) t = time(nullptr);
      struct tm* tmp = localtime(&t);
      char timeString[64];
      std::sprintf(timeString, "%04d-%02d-%02d %02d:%02d:%02d.%03d", 1900 + tmp->tm_year, tmp->tm_mon + 1, tmp->tm_mday,
          tmp->tm_hour, tmp->tm_min, tmp->tm_sec, static_cast<int>(tv.tv_usec / 1000));

      // create groups
      currentGroupName = std::string("/") + std::string(timeString);
      try {
        outFile->createGroup(currentGroupName);
        for(auto& group : groupList) outFile->createGroup(currentGroupName + "/" + group);
        outFile->createGroup(currentGroupName + "/MicroDAQ");
      }
      catch(H5::FileIException&) {
        outFile->close();
        isOpened = false; // will re-open file on next trigger
        return;
      }

      // write all data to file
      boost::fusion::for_each(_owner->BaseDAQ<TRIGGERTYPE>::_accessorListMap.table, H5DataWriter<TRIGGERTYPE>(*this));

      // write internal data
      // ToDo: userTypeToNumeric<float>((TRIGGERTYPE)_owner->BaseDAQ<TRIGGERTYPE>::status.nMissedTriggers) is not
      // working for Boolean - Why?
      TRIGGERTYPE tmpData = _owner->BaseDAQ<TRIGGERTYPE>::status.nMissedTriggers;
      _buffer[0] = userTypeToNumeric<float>(tmpData);
      H5::DataSet dataset{outFile->createDataSet(currentGroupName + "/MicroDAQ/nMissedTriggers",
          H5::PredType::NATIVE_FLOAT, _space["MicroDAQ.nMissedTriggers"])};
      dataset.write(_buffer.data(), H5::PredType::NATIVE_FLOAT);
      H5::DataSet dataset1{outFile->createDataSet(
          currentGroupName + "/MicroDAQ/triggerPeriod", H5::PredType::NATIVE_FLOAT, _space["MicroDAQ.triggerPeriod"])};
      _buffer[0] = userTypeToNumeric<float>((int64_t)_owner->BaseDAQ<TRIGGERTYPE>::status.triggerPeriod);
      dataset1.write(_buffer.data(), H5::PredType::NATIVE_FLOAT);
    }

    /******************************************************************************************************************/

  } // namespace detail

  INSTANTIATE_TEMPLATE_FOR_CHIMERATK_USER_TYPES_NO_VOID(HDF5DAQ);

} // namespace ChimeraTK
