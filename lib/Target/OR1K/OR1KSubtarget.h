//=====-- OR1KSubtarget.h - Define Subtarget for the OR1K -----*- C++ -*--==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the OR1K specific subclass of TargetSubtarget.
//
//===----------------------------------------------------------------------===//

#ifndef OR1KSUBTARGET_H
#define OR1KSUBTARGET_H

#include "OR1KInstrInfo.h"
#include "OR1KISelLowering.h"
#include "OR1KFrameLowering.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SUBTARGETINFO_HEADER
#include "OR1KGenSubtargetInfo.inc"

namespace llvm {
class OR1KTargetMachine;

class OR1KSubtarget : public OR1KGenSubtargetInfo {
  virtual void anchor();
  bool HasMul;
  bool HasDiv;
  bool HasRor;
  bool HasCmov;
  bool HasAddc;
  bool HasFfl1;
  bool HasExt;

  const OR1KFrameLowering FrameLowering;
  const OR1KInstrInfo InstrInfo;
  const OR1KTargetLowering TLInfo;
  const SelectionDAGTargetInfo TSInfo;
public:
  /// This constructor initializes the data members to match that
  /// of the specified triple.
  ///
  OR1KSubtarget(const Triple &TT,
                const std::string &CPU, const std::string &FS,
                const OR1KTargetMachine &TM);

  OR1KSubtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS);

  /// ParseSubtargetFeatures - Parses features string setting specified
  /// subtarget options.  Definition of function is auto generated by tblgen.
  void ParseSubtargetFeatures(StringRef CPU, StringRef FS);

  bool hasMul()    const { return HasMul; }
  bool hasDiv()    const { return HasDiv; }
  bool hasRor()    const { return HasRor; }
  bool hasCmov()   const { return HasCmov; }
  bool hasAddc()   const { return HasAddc; }
  bool hasFfl1()   const { return HasFfl1; }
  bool hasExt()    const { return HasExt; }

  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const OR1KInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const OR1KRegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const OR1KTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }
};
} // End llvm namespace

#endif
