//===-- OR1KISelDAGToDAG.cpp - A dag to dag inst selector for OR1K --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the OR1K target.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "or1k-isel"
#include "OR1K.h"
#include "OR1KMachineFunctionInfo.h"
#include "OR1KRegisterInfo.h"
#include "OR1KSubtarget.h"
#include "OR1KTargetMachine.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// OR1KDAGToDAGISel - OR1K specific code to select OR1K machine
// instructions for SelectionDAG operations.
//===----------------------------------------------------------------------===//
namespace {

class OR1KDAGToDAGISel : public SelectionDAGISel {
  const OR1KSubtarget *Subtarget;

public:
  explicit OR1KDAGToDAGISel(OR1KTargetMachine &TM) : SelectionDAGISel(TM) {}

  StringRef getPassName() const override {
    return "OR1K DAG->DAG Pattern Instruction Selection";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  bool SelectInlineAsmMemoryOperand(const SDValue &Op, unsigned ConstraintID,
                                    std::vector<SDValue> &OutOps) override;

private:
  // Include the pieces autogenerated from the target description.
  #include "OR1KGenDAGISel.inc"

  void Select(SDNode *N) override;

  // Complex Pattern for address selection.
  bool SelectAddr(SDValue Addr, SDValue &Base, SDValue &Offset);
};

}

bool OR1KDAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  Subtarget = &static_cast<const OR1KSubtarget &>(MF.getSubtarget());
  return SelectionDAGISel::runOnMachineFunction(MF);
}

/// ComplexPattern used on OR1KInstrInfo
/// Used on OR1K Load/Store instructions
bool OR1KDAGToDAGISel::
SelectAddr(SDValue Addr, SDValue &Base, SDValue &Offset) {
  // if Address is FI, get the TargetFrameIndex.
  SDLoc dl(Addr);
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base   = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    Offset = CurDAG->getTargetConstant(0, dl, MVT::i32);
    return true;
  }

  // on PIC code Load GA
  if (TM.getRelocationModel() == Reloc::PIC_) {
    if ((Addr.getOpcode() == ISD::TargetGlobalAddress) ||
        (Addr.getOpcode() == ISD::TargetConstantPool) ||
        (Addr.getOpcode() == ISD::TargetJumpTable)){
      OR1KMachineFunctionInfo *MFI =
        CurDAG->getMachineFunction().getInfo<OR1KMachineFunctionInfo>();
      Base   = CurDAG->getRegister(MFI->getGlobalBaseReg(), MVT::i32);
      Offset = Addr;
      return true;
    }
  } else {
    if ((Addr.getOpcode() == ISD::TargetExternalSymbol ||
        Addr.getOpcode() == ISD::TargetGlobalAddress))
      return false;
  }

  // Addresses of the form FI+const or FI|const
  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1));
    if (isInt<16>(CN->getSExtValue())) {

      // If the first operand is a FI, get the TargetFI Node
      if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>
                                  (Addr.getOperand(0)))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
      else
        Base = Addr.getOperand(0);

      Offset = CurDAG->getTargetConstant(CN->getZExtValue(), SDLoc(CN),
                                         MVT::i32);
      return true;
    }
  }

  // Operand is a result from an ADD.
  if (Addr.getOpcode() == ISD::ADD) {
    if (ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
      if (isUInt<16>(CN->getZExtValue())) {

        // If the first operand is a FI, get the TargetFI Node
        if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>
                                    (Addr.getOperand(0))) {
          Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
        } else {
          Base = Addr.getOperand(0);
        }

        Offset = CurDAG->getTargetConstant(CN->getZExtValue(), SDLoc(CN),
                                           MVT::i32);
        return true;
      }
    }
  }

  // Fold the ORI in MOVHI->ORI->LW/SW chains into LW/SW, if possible.
  if (ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getNode())) {
    uint64_t AddrImm = CN->getZExtValue();
    // The LW/SW offset is sign-extended, and we want to avoid subtraction.
    if((AddrImm & 0x8000) == 0) {
      SDValue AddrHigh = CurDAG->getTargetConstant(AddrImm >> 16, dl, MVT::i32);
      Base = SDValue(CurDAG->getMachineNode(OR1K::MOVHI, dl, MVT::i32, AddrHigh), 0);
      Offset = CurDAG->getTargetConstant(AddrImm & 0x7FFF, dl, MVT::i32);
      return true;
    }
  }

  Base   = Addr;
  Offset = CurDAG->getTargetConstant(0, dl, MVT::i32);
  return true;
}

bool OR1KDAGToDAGISel::
SelectInlineAsmMemoryOperand(const SDValue &Op, unsigned ConstraintID,
                             std::vector<SDValue> &OutOps) {
  SDValue Op0, Op1;
  switch (ConstraintID) {
  default: return true;
  case InlineAsm::Constraint_m:   // memory
    if (!SelectAddr(Op, Op0, Op1))
      return true;
    break;
  }

  OutOps.push_back(Op0);
  OutOps.push_back(Op1);
  return false;
}

/// Select instructions not customized! Used for
/// expanded, promoted and normal instructions
void OR1KDAGToDAGISel::Select(SDNode *Node) {
  unsigned Opcode = Node->getOpcode();
  SDLoc dl(Node);

  // Dump information about the Node being selected
  DEBUG(errs() << "Selecting: "; Node->dump(CurDAG); errs() << "\n");

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    DEBUG(errs() << "== "; Node->dump(CurDAG); errs() << "\n");
    Node->setNodeId(-1);
    return;
  }

  ///
  // Instruction Selection not handled by the auto-generated
  // tablegen selection should be handled here.
  ///
  switch(Opcode) {
    default: break;

    case ISD::FrameIndex: {
      SDValue imm = CurDAG->getTargetConstant(0, dl, MVT::i32);
      int FI = dyn_cast<FrameIndexSDNode>(Node)->getIndex();
      EVT VT = Node->getValueType(0);
      SDValue TFI = CurDAG->getTargetFrameIndex(FI, VT);
      unsigned Opc = OR1K::ADDI;
      if (Node->hasOneUse())
        CurDAG->SelectNodeTo(Node, Opc, VT, TFI, imm);
      else
        ReplaceNode(Node, CurDAG->getMachineNode(Opc, dl, VT, TFI, imm));
      return;
    }
  }

  // Select the default instruction
  SelectCode(Node);
}

/// createOR1KISelDag - This pass converts a legalized DAG into a
/// OR1K-specific DAG, ready for instruction scheduling.
FunctionPass *llvm::createOR1KISelDag(OR1KTargetMachine &TM) {
  return new OR1KDAGToDAGISel(TM);
}
