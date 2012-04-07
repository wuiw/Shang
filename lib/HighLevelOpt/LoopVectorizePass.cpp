//===-- LoopVectorizePass.cpp - Simple Unroll-and-Vectorize Pass ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass implements a simple loop unroll-and-vectorize pass.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loop-unroll-vectorize"
#include "vtm/Passes.h"

#include "llvm/IntrinsicInst.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CodeMetrics.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Transforms/Vectorize.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Target/TargetData.h"
#include <climits>

using namespace llvm;
STATISTIC(NumLoopUnrollForVecotirze, "Number of loops unrolled for vectrizie");

namespace {
struct LoopVectorizer : public LoopPass {
  static char ID;
  ScalarEvolution *SE;
  TargetData *TD;

  LoopVectorizer() : LoopPass(ID), SE(0), TD(0) {
    initializeLoopVectorizerPass(*PassRegistry::getPassRegistry());
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AliasAnalysis>();
    AU.addRequired<LoopInfo>();
    AU.addPreserved<LoopInfo>();
    AU.addRequiredID(LoopSimplifyID);
    AU.addPreservedID(LoopSimplifyID);
    AU.addRequiredID(LCSSAID);
    AU.addPreservedID(LCSSAID);
    AU.addRequired<ScalarEvolution>();
    AU.addPreserved<ScalarEvolution>();
    // FIXME: Loop unroll requires LCSSA. And LCSSA requires dom info.
    // If loop unroll does not preserve dom info then LCSSA pass on next
    // loop will receive invalid dom info.
    // For now, recreate dom info, if loop is unrolled.
    AU.addPreserved<DominatorTree>();
  }

  virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
  unsigned analyzeLoop(Loop *L, const SCEV *BECount);
  unsigned processLoopMemOp(Value *Val, Value *Ptr, Loop *CurLoop);
};
}

Pass *llvm::createLoopVectorizerPass() {
  return new LoopVectorizer();
}

char LoopVectorizer::ID = 0;
INITIALIZE_PASS_BEGIN(LoopVectorizer, "loop-vectorize-unroll", "Unroll loops",
                      false, false)
INITIALIZE_AG_DEPENDENCY(AliasAnalysis)
INITIALIZE_PASS_DEPENDENCY(DominatorTree)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_DEPENDENCY(LoopSimplify)
INITIALIZE_PASS_DEPENDENCY(LCSSA)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolution)
INITIALIZE_PASS_END(LoopVectorizer, "loop-vectorize-unroll", "Unroll loops",
                    false, false)

/// processLoopStore - See if this store can be promoted to a memset or memcpy.
unsigned LoopVectorizer::processLoopMemOp(Value *Val, Value *Ptr, Loop *L){
  // Reject stores that are so large that they overflow an unsigned.
  uint64_t SizeInBits = TD->getTypeSizeInBits(Val->getType());
  if ((SizeInBits & 7) || (SizeInBits >> 32) != 0)
    return UINT_MAX;

  // See if the pointer expression is an AddRec like {base,+,1} on the current
  // loop, which indicates a strided store.  If we have something else, it's a
  // random store we can't handle.
  const SCEVAddRecExpr *PtrEv = dyn_cast<SCEVAddRecExpr>(SE->getSCEV(Ptr));
  if (PtrEv == 0 || PtrEv->getLoop() != L || !PtrEv->isAffine())
    return UINT_MAX;

  // Check to see if the stride matches the size of the store.  If so, then we
  // know that every byte is touched in the loop.
  unsigned SizeInBytes = (unsigned)SizeInBits >> 3;
  const SCEVConstant *Stride = dyn_cast<SCEVConstant>(PtrEv->getOperand(1));

  if (Stride == 0 || SizeInBytes != Stride->getValue()->getValue()) {
    // TODO: Could also handle negative stride here someday, that will require
    // the validity check in mayLoopAccessLocation to be updated though.
    // Enable this to print exact negative strides.
    if (0 && Stride && SizeInBytes == -Stride->getValue()->getValue()) {
      dbgs() << "NEGATIVE STRIDE: " << *Ptr << "\n";
      //dbgs() << "BB: " << *Ptr->getParent();
    }

    return UINT_MAX;
  }

  return SizeInBytes;
}


unsigned LoopVectorizer::analyzeLoop(Loop *L, const SCEV *BECount) {
  BasicBlock *BB = L->getHeader();
  unsigned MemOpSize = UINT_MAX;
  for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
    Instruction *Inst = I;
    // Look for store instructions, which may be optimized to memset/memcpy.
    if (StoreInst *SI = dyn_cast<StoreInst>(Inst)) {
      if (!SI->isSimple()) return 0;

      MemOpSize = std::min(processLoopMemOp(SI->getValueOperand(),
                                            SI->getPointerOperand(),
                                            L),
                           MemOpSize);
      continue;
    }

    if (LoadInst *LI = dyn_cast<LoadInst>(Inst)) {
      if (!LI->isSimple()) return 0;

      MemOpSize = std::min(processLoopMemOp(LI, LI->getPointerOperand(),
                                            L),
                           MemOpSize);
      continue;
    }

  }

  return MemOpSize;
}

bool LoopVectorizer::runOnLoop(Loop *L, LPPassManager &LPM) {
  // Only handle the loop with single BB at the moment.
  if (L->getNumBlocks() > 1) return false;

  // The trip count of the loop must be analyzable.
  SE = &getAnalysis<ScalarEvolution>();
  if (!SE->hasLoopInvariantBackedgeTakenCount(L))
    return false;
  const SCEV *BECount = SE->getBackedgeTakenCount(L);
  if (isa<SCEVCouldNotCompute>(BECount)) return false;

  unsigned TripCount = 0;
  unsigned TripMultiple = 1;
  // Find "latch trip count". UnrollLoop assumes that control cannot exit
  // via the loop latch on any iteration prior to TripCount. The loop may exit
  // early via an earlier branch.
  BasicBlock *LatchBlock = L->getLoopLatch();
  if (LatchBlock) {
    TripCount = SE->getSmallConstantTripCount(L, LatchBlock);
    TripMultiple = SE->getSmallConstantTripMultiple(L, LatchBlock);
  } else
    return false;

  // If this loop executes exactly one time, then it should be peeled, not
  // optimized by this pass.
  if (const SCEVConstant *BECst = dyn_cast<SCEVConstant>(BECount)) {
    if (BECst->getValue()->getValue() == 0) return false;
  }

  // We require target data for now.
  TD = getAnalysisIfAvailable<TargetData>();
  if (TD == 0) return false;

  unsigned MinimalAccessSize = analyzeLoop(L, BECount);
  unsigned UnrollCount = 8 / MinimalAccessSize;
  if (UnrollCount <= 1 || TripCount % UnrollCount != 0) return false;

  LoopInfo *LI = &getAnalysis<LoopInfo>();
  // Unroll the loop.
  if (!UnrollLoop(L, UnrollCount, TripCount, false, TripMultiple, LI, &LPM))
    return false;

  ++NumLoopUnrollForVecotirze;
  return true;
}