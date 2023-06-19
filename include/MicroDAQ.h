/*
 * MicroDAQ.h
 *
 *  Created on: Feb 3, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#pragma once

#include <ChimeraTK/ApplicationCore/ApplicationModule.h>
#include <ChimeraTK/ApplicationCore/ArrayAccessor.h>
#include <ChimeraTK/ApplicationCore/ConfigReader.h>
#include <ChimeraTK/ApplicationCore/HierarchyModifyingGroup.h>
#include <ChimeraTK/ApplicationCore/ModuleGroup.h>
#include <ChimeraTK/ApplicationCore/ScalarAccessor.h>
#include <ChimeraTK/ApplicationCore/VariableGroup.h>

#include <boost/filesystem.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

namespace ChimeraTK {

  template<typename TRIGGERTYPE>
  class BaseDAQ;

  /********************************************************************************************************************/

  /**
   *  Envelope class providing an easy-to-use high-level application interface. Instantiate this class in your
   *  application to use the MicroDAQ. It will configure itself using the ConfigReader (needs to be instantiated
   *  in your application before the MicroDAQ).
   */
  template<typename TRIGGERTYPE>
  class MicroDAQ : public ModuleGroup {
   public:
    /**
     *  Construct MicroDAQ system as it is configured in the ConfigReader config file.
     *
     *  As data source all variables provided by the owner and its submodules matching the specified inputTag is used.
     *
     *  In the config file, the following variables are required:
     *  - Configuration/MicroDAQ/enable (int32): boolean flag whether the MicroDAQ system is enabled or not
     *  - Configuration/MicroDAQ/outputFormat (string): format of the output data, either "hdf5" or "root"
     *  - Configuration/MicroDAQ/decimationFactor (uint32): decimation factor applied to large arrays (above
     *    decimationThreshold)
     *  - Configuration/MicroDAQ/decimationThreshold (uint32): array size threshold above which the decimationFactor is
     *    applied
     *
     *  If Configuration/MicroDAQ/enable == 0, all other variables can be omitted.
     */
    MicroDAQ(ModuleGroup* owner, const std::string& name, const std::string& description, const std::string& inputTag,
        const std::string& pathToTrigger, const std::unordered_set<std::string>& tags = {});
    MicroDAQ() = default;

    std::shared_ptr<BaseDAQ<TRIGGERTYPE>> getImplementation() { return impl; }

    /**
     * Add variable of a DeviceModule directly to the DAQ.
     * \param source The Device module to consider.
     * \param namePrefix This prefix is used in the root file tree. It is prepended to the register names from the
     *        device.
     * \param submodule Use only a submodule of the device.
     */
    void addDeviceModule(DeviceModule& source, const RegisterPath& namePrefix = "", const RegisterPath& submodule = "");

   protected:
    std::shared_ptr<BaseDAQ<TRIGGERTYPE>> impl;
  };

  /********************************************************************************************************************/

  /**
   *  Base class used by the actual MicroDAQ implementations (HDF5, ROOT)
   */
  template<typename TRIGGERTYPE>
  class BaseDAQ : public ApplicationModule {
   private:
    // Required by public member initialisers, hence define first
    // Tag name to tag control variables published by the MicroDAQ module itself, to exclude them from being added to
    // the DAQ as a source.
    const std::string _tagExcludeInternals{"_ChimeraTK_BaseDAQ_controlVars"};

   public:
    BaseDAQ(ModuleGroup* owner, const std::string& name, const std::string& description, const std::string& suffix,
        uint32_t decimationFactor = 10, uint32_t decimationThreshold = 1000,
        const std::unordered_set<std::string>& tags = {}, const std::string& pathToTrigger = "trigger")
    : ApplicationModule(owner, name, description, tags), trigger(this, pathToTrigger, "", "", {_tagExcludeInternals}),
      _decimationFactor(decimationFactor), _decimationThreshold(decimationThreshold),
      _daqDefaultPath((boost::filesystem::current_path() / "uDAQ").string()), _suffix(suffix) {}

    BaseDAQ() : _decimationFactor(0), _decimationThreshold(0) {}

    /**
     * Check if accessor uses the DAQ trigger as external trigger.
     *
     * In that case it has to be updated as well as the trigger before reading data by the DAQ.
     */
    template<typename UserType>
    bool isAccessorUsingDAQTrigger(const ArrayPushInput<UserType>& accessor);

    ScalarPushInput<TRIGGERTYPE> trigger;

    ScalarPollInput<std::string> setPath{this, "directory",
        "", "Directory where to store the DAQ data. If not set a subdirectory called uDAQ in the current directory is used.",
        {_tagExcludeInternals}};

    ScalarPollInput<ChimeraTK::Boolean> enable{this, "activate", "", "Activate the DAQ.", {_tagExcludeInternals}};

    ScalarPollInput<uint32_t> nMaxFiles{this, "nMaxFiles", "",
        "Maximum number of files in the ring buffer "
        "(oldest file will be overwritten).",
        {_tagExcludeInternals}};

    ScalarPollInput<uint32_t> nTriggersPerFile{
        this, "nTriggersPerFile", "", "Number of triggers stored in each file.", {_tagExcludeInternals}};

    ScalarOutput<std::string> currentPath{this, "currentDirectory", "",
        "Directory currently used for DAQ. To switch directories turn off DAQ and set new directory.",
        {_tagExcludeInternals}};

    ScalarOutput<uint32_t> currentBuffer{this, "currentBuffer", "",
        "File number currently written to. If DAQ this shows the next buffer to be used by the DAQ.",
        {_tagExcludeInternals}};

    ScalarOutput<uint32_t> currentEntry{
        this, "currentEntry", "", "Last entry number written. Is reset with every new file.", {_tagExcludeInternals}};

    ScalarOutput<ChimeraTK::Boolean> errorStatus{
        this, "DAQError", "", "True in case an error occurred. Reset by toggling enable.", {_tagExcludeInternals}};

    /**
     * Add all PVs found below the given directory.
     *
     * @param qualifiedDirectoryPath qualified name of the directory
     * @param inputTag name of the tag to select. If empty, use all PVs.
     */
    void addSource(const std::string& qualifiedDirectoryPath, const std::string& inputTag);

    void mainLoop() override = 0;

    void prepare() override;

    /**
     * Visitor function for use with the ApplicationCore Model to add PVs as DAQ sources
     */
    void addVariableFromModel(const ChimeraTK::Model::ProcessVariableProxy& pv, const RegisterPath& namePrefix = "",
        const RegisterPath& submodule = "");

   protected:
    /** Parameters for the data decimation */
    uint32_t _decimationFactor, _decimationThreshold;

    boost::filesystem::path _daqPath; ///< DAQ path

    std::string _daqDefaultPath; ///< Default DAQ path stored to check easily for new DAQ path

    std::string _suffix; ///< The DAQ type specific suffix e.g. .root

    std::string _prefix; ///< Current prefix path+date

    /** Overall variable name list, used to detect name collisions */
    std::set<std::string> _overallVariableList;

    /**
     * boost::fusion::map of UserTypes to std::lists containing the names of the accessors. Technically there would be
     * no need to use TemplateUserTypeMap for this (as type does not depend on the UserType), but since these lists must
     * be filled consistently with the accessorListMap, the same construction is used here.
     */
    template<typename UserType>
    using NameList = std::list<std::string>;
    TemplateUserTypeMapNoVoid<NameList> _nameListMap;

    /**
     * boost::fusion::map of UserTypes to std::lists containing the ArrayPushInput accessors. These accessors are
     * dynamically created by the AccessorAttacher.
     */
    template<typename UserType>
    using AccessorList = std::list<ArrayPushInput<UserType>>;
    TemplateUserTypeMapNoVoid<AccessorList> _accessorListMap;

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
  };

  /********************************************************************************************************************/

  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES_NO_VOID(BaseDAQ);

  /********************************************************************************************************************/
  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  template<typename UserType>
  bool BaseDAQ<TRIGGERTYPE>::isAccessorUsingDAQTrigger(const ArrayPushInput<UserType>& accessor) {
    // Obtain the path of the trigger for the given accessor (stays empty if no trigger)
    std::string triggerPath;
    auto visitor = [&](const Model::DeviceModuleProxy& dev) { triggerPath = dev.getTrigger().getFullyQualifiedPath(); };
    accessor.getModel().visit(visitor, Model::keepDeviceModules, Model::adjacentInSearch, Model::keepPvAccess);

    // compare to the path of the DAQ trigger
    return triggerPath == trigger.getModel().getFullyQualifiedPath();
  }

  /********************************************************************************************************************/

  template<typename TRIGGERTYPE>
  void BaseDAQ<TRIGGERTYPE>::addVariableFromModel(
      const ChimeraTK::Model::ProcessVariableProxy& pv, const RegisterPath& namePrefix, const RegisterPath& submodule) {
    // do not add control variables published by the MicroDAQ module itself
    if(pv.getTags().count(_tagExcludeInternals) > 0) {
      return;
    }

    // gather information about the PV
    auto name = pv.getFullyQualifiedPath();
    const auto& type = pv.getNodes().front().getValueType(); // All node types must be equal for a PV
    auto length = pv.getNodes().front().getNumberOfElements();

    // check if qualified path name patches the given submodule name
    if(submodule != "/" && !boost::starts_with(name, std::string(submodule) + "/")) {
      return;
    }

    // generate name as visible in the DAQ
    std::string daqName = namePrefix / name.substr(submodule.length());

    // check for name collision
    if(_overallVariableList.count(daqName) > 0) {
      // Can happen if a pv is added in the logical name mapping process twice, e.g. to use math plugin
      return;
    }
    _overallVariableList.insert(daqName);

    // create accessor and fill lists
    callForTypeNoVoid(type, [&](auto t) {
      using UserType = decltype(t);
      boost::fusion::at_key<UserType>(_nameListMap.table).push_back(daqName);
      boost::fusion::at_key<UserType>(_accessorListMap.table).emplace_back(this, name, "", length, "");
    });
  }

  /********************************************************************************************************************/

} // namespace ChimeraTK
