// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * data_type.h
 *
 *  Created on: Feb 22, 2020
 *      Author: Klaus Zenker (HZDR)
 */

#include "TArrayC.h"
#include "TArrayD.h"
#include "TArrayF.h"
#include "TArrayI.h"
#include "TArrayL.h"
#include "TArrayS.h"
#include "TDatime.h"
#include "TString.h"

#include <ChimeraTK/SupportedUserTypes.h>

#include <map>
#include <string>

/** MicroDAQ related data types */
namespace ChimeraTK { namespace detail {
  /* Add TArray for std::string since it is needed
   * for the TreeDataFields template specialization.
   */
  class TArrayStr : public TArray {
   private:
    std::vector<std::string> _buffer;

   public:
    Double_t GetAt(Int_t i) const override { return std::stod(_buffer.at(i)); }
    void Set(Int_t i) override {
      _buffer.resize(i);
      fN = i;
    }
    void SetAt(Double_t, Int_t) override {}
    std::string& operator[](Int_t i) { return _buffer.at(i); }
    std::string operator[](Int_t i) const { return _buffer.at(i); }
    std::string At(Int_t i) const { return _buffer.at(i); }
  };

  template<typename UserType>
  struct TreeDataFields {};
  template<>
  struct TreeDataFields<float> {
    std::map<std::string, Float_t> parameter;
    std::map<std::string, TArrayF> trace;
  };
  template<>
  struct TreeDataFields<double> {
    std::map<std::string, Double_t> parameter;
    std::map<std::string, TArrayD> trace;
  };
  template<>
  struct TreeDataFields<uint8_t> {
    std::map<std::string, UChar_t> parameter;
    std::map<std::string, TArrayS> trace;
  };
  template<>
  struct TreeDataFields<int8_t> {
    std::map<std::string, Char_t> parameter;
    std::map<std::string, TArrayS> trace;
  };
  template<>
  struct TreeDataFields<uint16_t> {
    std::map<std::string, UShort_t> parameter;
    std::map<std::string, TArrayS> trace;
  };
  template<>
  struct TreeDataFields<int16_t> {
    std::map<std::string, Short_t> parameter;
    std::map<std::string, TArrayS> trace;
  };
  template<>
  struct TreeDataFields<uint32_t> {
    std::map<std::string, UInt_t> parameter;
    std::map<std::string, TArrayI> trace;
  };
  template<>
  struct TreeDataFields<int32_t> {
    std::map<std::string, Int_t> parameter;
    std::map<std::string, TArrayI> trace;
  };
  template<>
  struct TreeDataFields<uint64_t> {
    std::map<std::string, ULong64_t> parameter;
    std::map<std::string, TArrayL> trace;
  };
  template<>
  struct TreeDataFields<int64_t> {
    std::map<std::string, Long64_t> parameter;
    std::map<std::string, TArrayL> trace;
  };

  template<>
  struct TreeDataFields<std::string> {
    std::map<std::string, std::string> parameter;
    std::map<std::string, TArrayStr> trace;
  };

  template<>
  struct TreeDataFields<Boolean> {
    std::map<std::string, Bool_t> parameter;
    std::map<std::string, TArrayC> trace;
  };
}} // namespace ChimeraTK::detail
