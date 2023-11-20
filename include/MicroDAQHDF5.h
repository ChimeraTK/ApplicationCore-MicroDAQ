// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * MicroDAQHDF5.h
 *
 *  Created on: 21.03.2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "MicroDAQ.h"

#include <ChimeraTK/ApplicationCore/ArrayAccessor.h>
#include <ChimeraTK/ApplicationCore/VariableGroup.h>
#include <ChimeraTK/SupportedUserTypes.h>

namespace ChimeraTK {

  namespace detail {
    template<typename TRIGGERTYPE>
    struct H5storage;
    template<typename TRIGGERTYPE>
    struct H5DataSpaceCreator;
    template<typename TRIGGERTYPE>
    struct H5DataWriter;
  } // namespace detail

  /********************************************************************************************************************/

  /**
   *  MicroDAQ module for logging data to HDF5 files. This can be usefull in
   * enviromenents where no sufficient logging of data is possible through the
   * control system. Any ChimeraTK::Module can act as a data source. Which
   * variables should be logged can be selected through EntityOwner::findTag().
   */
  template<typename TRIGGERTYPE = int32_t>
  class HDF5DAQ : public BaseDAQ<TRIGGERTYPE> {
   public:
    /**
     *  Constructor. decimationFactor and decimationThreshold are configuration
     * constants which determine how the data reduction is working. Arrays with a
     * size bigger than decimationThreshold will be decimated by decimationFactor
     * before writing to the HDF5 file.
     */
    HDF5DAQ(ModuleGroup* owner, const std::string& name, const std::string& description, uint32_t decimationFactor = 10,
        uint32_t decimationThreshold = 1000, const std::unordered_set<std::string>& tags = {},
        const std::string& pathToTrigger = "trigger")
    : BaseDAQ<TRIGGERTYPE>(
          owner, name, description, ".h5", decimationFactor, decimationThreshold, tags, pathToTrigger) {}

    /** Default constructor, creates a non-working module. Can be used for late
     * initialisation. */
    HDF5DAQ() : BaseDAQ<TRIGGERTYPE>() {}

   protected:
    void mainLoop() override;

    friend struct detail::H5storage<TRIGGERTYPE>;
    friend struct detail::H5DataSpaceCreator<TRIGGERTYPE>;
    friend struct detail::H5DataWriter<TRIGGERTYPE>;
  };

  /********************************************************************************************************************/

  DECLARE_TEMPLATE_FOR_CHIMERATK_USER_TYPES_NO_VOID(HDF5DAQ);

  /********************************************************************************************************************/

} // namespace ChimeraTK
