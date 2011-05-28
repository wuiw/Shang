//===---------- VInstrInfo.cpp - VTM Instruction Information -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the VTM implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "VTargetMachine.h"

#include "vtm/VInstrInfo.h"
#include "vtm/VTM.h"

#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ErrorHandling.h"

#include "VGenInstrInfo.inc"

using namespace llvm;

VInstrInfo::VInstrInfo(const TargetData &TD, const TargetLowering &TLI)
  : TargetInstrInfoImpl(VTMInsts, array_lengthof(VTMInsts)), RI(*this, TD, TLI)
  {}

bool VInstrInfo::AnalyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                               MachineBasicBlock *&FBB,
                               SmallVectorImpl<MachineOperand> &Cond,
                               bool AllowModify /* = false */) {
  // TODO: Write code for this function.
  return true;
}

FuncUnitId VInstr::getPrebindFUId()  const {
  // Dirty Hack: Bind all memory access to channel 0 at this moment.
  if (getTID().Opcode == VTM::VOpMemTrans)
    return FuncUnitId(VFUs::MemoryBus, 0);

  if (getTID().Opcode == VTM::VOpBRam) {
    unsigned Id = I.getOperand(5).getImm();
    return FuncUnitId(VFUs::BRam, Id);
  }

  return FuncUnitId();
}


BitWidthAnnotator::BitWidthAnnotator(MachineInstr &MI)
  : MO(&MI.getOperand(MI.getNumOperands() - 1)), BitWidths(MO->getImm()) {
  assert(hasBitWidthInfo() && "BitWidthAnnotator not available anymore!");
}

void BitWidthAnnotator::updateBitWidth() {
  assert(MO && "Cannot update bit width!");
  MO->setImm(BitWidths);
}

bool BitWidthAnnotator::hasBitWidthInfo() const {
  assert(MO && "MachineOperand not available");
  return MO->getTargetFlags() == 0;
}

void BitWidthAnnotator::changeToDefaultPred() {

}

bool VInstr::mayLoad() const {
  switch (getTID().Opcode) {
  default: return false;
  // There is a "isLoad" flag in memory access operation.
  case VTM::VOpMemTrans: return !I.getOperand(3).getImm();
  }
}

bool VInstr::mayStore() const {
  switch (getTID().Opcode) {
  default: return false;
    // There is a "isLoad" flag in memory access operation.
  case VTM::VOpMemTrans: return I.getOperand(3).getImm();
  }
}

bool VInstr::canCopyBeFused() const {
  assert(I.isCopy() && "canCopyBeFused called on the wrong instruction!");
  if (I.getOperand(1).isImm()) return true;

  assert(I.getParent() && "Expected instruction embedded in machine function!");
  const MachineRegisterInfo &MRI = I.getParent()->getParent()->getRegInfo();
  unsigned DstReg = I.getOperand(0).getReg(),
           SrcReg = I.getOperand(1).getReg();

  // Later pass can not eliminate the non-trivial copy, so it should be fused.
  return MRI.getRegClass(DstReg) != MRI.getRegClass(SrcReg);
}

// Out of line virtual function to provide home for the class.
void VInstr::anchor() {}
