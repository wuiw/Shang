//===-- vbe/TestbenchWriter.h - Testbench auto generation --------*- C++ -*-===//
//
//                            The Verilog Backend
//
// Copyright: 2010 by Hongbin Zheng. all rights reserved.
// IMPORTANT: This software is supplied to you by Hongbin Zheng in consideration
// of your agreement to the following terms, and your use, installation, 
// modification or redistribution of this software constitutes acceptance
// of these terms.  If you do not agree with these terms, please do not use, 
// install, modify or redistribute this software. You may not redistribute, 
// install copy or modify this software without written permission from 
// Hongbin Zheng. 
//
//===----------------------------------------------------------------------===//
//
// This class generate Testbench for verilog design automatically. 
//
//===----------------------------------------------------------------------===//
#ifndef VBE_TESTBENCH_WRITER
#define VBE_TESTBENCH_WRITER

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace esyn {
class VLang;

class TestbenchWriter : public FunctionPass{
    raw_ostream &Out;
    VLang *vlang;
public:
    static char ID; 

    explicit TestbenchWriter(raw_ostream &O)
      :FunctionPass(&ID),Out(O) {}

    void printFunctionSignature(raw_ostream &Out, const Function *F);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnFunction(Function &F);

    std::string testBech(Function&F);
  };

} //end namespace


#endif
