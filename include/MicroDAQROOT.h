/*
 * MicroDAQROOT.h
 *
 *  Created on: 21.03.2020
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_MICRODAQROOT_H_
#define INCLUDE_MICRODAQROOT_H_

#include <ChimeraTK/SupportedUserTypes.h>
#include <ChimeraTK/ApplicationCore/ArrayAccessor.h>
#include <ChimeraTK/ApplicationCore/VariableGroup.h>


#include "MicroDAQ.h"

namespace ChimeraTK {

  namespace detail {
    template<typename TRIGGERTYPE>
    struct ROOTstorage;
    template<typename TRIGGERTYPE>
    struct ROOTDataSpaceCreator;
    template<typename TRIGGERTYPE>
    struct ROOTTreeCreator;
    template<typename TRIGGERTYPE>
    struct ROOTDataWriter;

  } // namespace detail
  /**
   *  MicroDAQ module for logging data to ROOT files. This can be useful in
   * environments where no sufficient logging of data is possible through the
   * control system. Any ChimeraTK::Module can act as a data source. Which
   * variables should be logged can be selected through EntityOwner::findTag().
   */
  template<typename TRIGGERTYPE = int32_t>
  class RootDAQ : public BaseDAQ<TRIGGERTYPE> {
    public:
     /**
      *  Constructor. decimationFactor and decimationThreshold are configuration
      * constants which determine how the data reduction is working. Arrays with a
      * size bigger than decimationThreshold will be decimated by decimationFactor
      * before writing to the ROOT file.
      */
      RootDAQ(EntityOwner* owner, const std::string& name, const std::string& description,
             uint32_t decimationFactor = 10, uint32_t decimationThreshold = 1000, HierarchyModifier hierarchyModifier = HierarchyModifier::none,
             const std::unordered_set<std::string>& tags = {})
             : BaseDAQ<TRIGGERTYPE>(owner, name, description, ".root", decimationFactor, decimationThreshold, hierarchyModifier, tags) {}

     /** Default constructor, creates a non-working module. Can be used for late
      * initialisation. */
     RootDAQ() : BaseDAQ<TRIGGERTYPE>() {}

     ScalarPollInput<uint32_t> flushAfterNEntries { this, "flushAfterNEntries", "",
         "Number of entries to be accumulated before writing to file. This is ignored if value is 0.", { "MicroDAQ.CONFIG" } };

    protected:
     void mainLoop() override;

     /** Branch names - is overallVariableList with '/' replaced by '.' */
     template<typename UserType>
     using BranchList = std::list<std::string>;
     TemplateUserTypeMap<BranchList>  _branchNameList;


     friend struct detail::ROOTstorage<TRIGGERTYPE>;
     friend struct detail::ROOTDataSpaceCreator<TRIGGERTYPE>;
     friend struct detail::ROOTTreeCreator<TRIGGERTYPE>;
     friend struct detail::ROOTDataWriter<TRIGGERTYPE>;
  };
  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES(RootDAQ);
} // namespace ChimeraTK



#endif /* INCLUDE_MICRODAQROOT_H_ */
