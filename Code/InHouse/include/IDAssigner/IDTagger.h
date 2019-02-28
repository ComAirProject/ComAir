#ifndef COMAIR_IDTAGGER_H
#define COMAIR_IDTAGGER_H

#include "llvm/Pass.h"

using namespace llvm;
using namespace std;


struct IDTagger: public ModulePass
{
	static char ID;
	IDTagger();

	vector<Loop *> AllLoops;

	virtual void getAnalysisUsage(AnalysisUsage &AU) const;
	virtual bool runOnModule(Module &M);

	void tagLoops(Module &M);
};

#endif  //COMAIR_IDTAGGER_H
