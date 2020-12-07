/*
 * MicroDAQ.h
 *
 *  Created on: Feb 3, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_MICRODAQ_H_
#define INCLUDE_MICRODAQ_H_

#include <ChimeraTK/ApplicationCore/ApplicationModule.h>
#include <ChimeraTK/ApplicationCore/ScalarAccessor.h>
#include <ChimeraTK/ApplicationCore/ArrayAccessor.h>
#include <ChimeraTK/ApplicationCore/VariableGroup.h>

#include <boost/filesystem.hpp>

#include <map>

namespace ChimeraTK {
  namespace detail {
    template<typename TRIGGERTYPE>
    struct BaseDAQAccessorAttacher;
  } // namespace detail

  template<typename TRIGGERTYPE>
  class BaseDAQ: public ApplicationModule {
  private:

    /**
     * Delete file corresponding to currentBuffer from the ringbuffer.
     */
    void deleteRingBufferFile();


    /**
     * Check if ringbuffer file corresponding to currentBuffer exists.
     *
     * Since timestamps are prefixed to the buffer files here we check
     * the last part of the file.
     *
     * \return True if file with currentBuffer number exists.
     */
    bool checkFile();

  protected:
    /** Parameters for the data decimation */
    uint32_t _decimationFactor, _decimationThreshold;

    boost::filesystem::path _daqPath; ///< DAQ path

    std::string _daqDefaultPath; ///< Default DAQ path stored to check easily for new DAQ path

    std::string _suffix;///< The DAQ type specific suffix e.g. .root

    std::string _prefix; ///< Current prefix path+date

    /** Map of VariableGroups required to build the hierarchies. The key it the
     * full path name. */
    std::map<std::string, VariableGroup> _groupMap;

    /** boost::fusion::map of UserTypes to std::lists containing the names of the
     * accessors. Technically there would be no need to use TemplateUserTypeMap
     * for this (as type does not depend on the UserType), but since these lists
     * must be filled consistently with the accessorListMap, the same construction
     * is used here. */
    template<typename UserType>
    using NameList = std::list<std::string>;
    TemplateUserTypeMap<NameList> _nameListMap;

    /** Overall variable name list, used to detect name collisions */
    std::list<std::string> _overallVariableList;

    /** boost::fusion::map of UserTypes to std::lists containing the
     * ArrayPushInput accessors. These accessors are dynamically created by the
     * AccessorAttacher. */
    template<typename UserType>
    using AccessorList = std::list<ArrayPushInput<UserType>>;
    TemplateUserTypeMap<AccessorList> _accessorListMap;

    template<typename UserType>
    VariableNetworkNode getAccessor(const std::string& variableName);

    /**
     * Set the daq path.
     *
     * Use setPath to set up daqPath. If succeeded set currentPath accordingly.
     */
    void setDAQPath();

    /**
     * Read current buffer from buffer file and check if
     * that DAQ file already exists.
     */
    void checkBufferOnFirstTrigger();

    /**
     * Construct file name of the next buffer and update buffer file.
     */
    std::string nextBuffer();

    /**
     * Update DAQ path in case user changed it.
     *
     * This is only done if DAQ is off!
     */
    void updateDAQPath();


    /**
     * Check if maximum number of entries per file is reached.
     *
     * In that case currentBuffer is increased and currentEntry is reset.
     *
     * \return True if maximum entries are reached. Derived DAQ classed should
     * close the file.
     *
     */
    bool maxEntriesReached();

    /**
     * Reset error state, currentBuffer, currentEntry.
     */
    void disableDAQ();


  public:
    BaseDAQ(EntityOwner* owner, const std::string& name, const std::string& description, const std::string &suffix,
            uint32_t decimationFactor = 10, uint32_t decimationThreshold = 1000,
            HierarchyModifier hierarchyModifier = HierarchyModifier::none, const std::unordered_set<std::string>& tags = {})
        : ApplicationModule(owner, name, description, hierarchyModifier, tags), _decimationFactor(decimationFactor),
          _decimationThreshold(decimationThreshold),
          _daqDefaultPath((boost::filesystem::current_path()/"uDAQ").string()),
          _suffix(suffix) {}

    BaseDAQ() :
        _decimationFactor(0), _decimationThreshold(0) {
    }

    ScalarPushInput<TRIGGERTYPE> trigger { this, "trigger", "",
        "When written, the MicroDAQ write snapshot of all variables "
            "to the file" };

    ScalarPollInput<std::string> setPath { this, "directory", "",
        "Directory where to store the DAQ data. If not set a subdirectory called uDAQ in the current directory is used.",
        { "MicroDAQ.CONFIG" } };

    ScalarPollInput<int> enable { this, "enable", "",
        "DAQ is active when set to 0 and disabled when set to 0.",
        { "MicroDAQ.CONFIG" } };

    ScalarPollInput<uint32_t> nMaxFiles { this, "nMaxFiles", "",
        "Maximum number of files in the ring buffer "
        "(oldest file will be overwritten).",
        { "MicroDAQ.CONFIG" } };

    ScalarPollInput<uint32_t> nTriggersPerFile { this, "nTriggersPerFile", "",
        "Number of triggers stored in each file.",
        { "MicroDAQ.CONFIG" } };

    ScalarOutput<std::string> currentPath { this, "currentDirectory", "",
        "Directory currently used for DAQ. To switch directories turn off DAQ and set new directory.",
        { "MicroDAQ.STATUS" } };

    ScalarOutput<uint32_t> currentBuffer { this, "currentBuffer", "",
        "File number currently written to. If DAQ this shows the next buffer to be used by the DAQ.",
        { "MicroDAQ.STATUS" } };

    ScalarOutput<uint32_t> currentEntry { this, "currentEntry", "",
        "Last entry number written. Is reset with every new file.",
        { "MicroDAQ.STATUS" } };

    ScalarOutput<uint32_t> errorStatus { this, "DAQError", "",
        "True in case an error occurred. Reset by toggling enable.",
        { "MicroDAQ.STATUS" } };

    virtual void addSource(const Module& source, const RegisterPath& namePrefix = "");

    virtual void mainLoop() = 0;

    friend struct detail::BaseDAQAccessorAttacher<TRIGGERTYPE>;
  };
  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(BaseDAQ);
} // namespace ChimeraTK

#endif /* INCLUDE_MICRODAQ_H_ */
