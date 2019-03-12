#include "FunctionWrapper.hpp"
#include "llvm/ADT/Twine.h"

using namespace llvm;

pdg::FunctionWrapper::FunctionWrapper(Function *Func)
{
  this->Func = Func;
  this->entryW = nullptr;
  for (Function::arg_iterator argIt = Func->arg_begin(), argE = Func->arg_end();
       argIt != argE;
       ++argIt)
  {
    ArgumentWrapper *argW = new ArgumentWrapper(&*argIt);
    argWList.push_back(argW);
  }

  const Twine t = "ret";
  Argument *ret = new Argument(Func->getReturnType(), t, Func, 100);
  this->retW = new ArgumentWrapper(ret);
}

bool pdg::FunctionWrapper::hasTrees()
{
  return treeFlag;
}

bool pdg::FunctionWrapper::isVisited()
{
  return visited;
}

pdg::ArgumentWrapper *pdg::FunctionWrapper::getArgWByArg(Argument &arg)
{
  if (arg.getArgNo() == 100)
    return retW;

  for (auto argW : argWList)
  {
    if (argW->getArg() == &arg)
    {
      return argW;
    }
  }

  errs() << "ERROR: cannot find argW by arg in Function " << Func->getName() << "\n";
  return nullptr;
}

void pdg::FunctionWrapper::addStoreInst(Instruction *inst)
{
  if (StoreInst *st = dyn_cast<StoreInst>(inst))
    storeInstList.push_back(st);
}

void pdg::FunctionWrapper::addLoadInst(Instruction *inst)
{
  if (LoadInst *li = dyn_cast<LoadInst>(inst))
    loadInstList.push_back(li);
}

void pdg::FunctionWrapper::addCallInst(Instruction *inst)
{
  if (CallInst *ci = dyn_cast<CallInst>(inst))
    callInstList.push_back(ci);
}

void pdg::FunctionWrapper::addBitCastInst(Instruction *inst)
{
  if (BitCastInst *bci = dyn_cast<BitCastInst>(inst))
    bitCastInstList.push_back(bci);
}

void pdg::FunctionWrapper::addIntrinsicInst(Instruction *inst)
{
  if (IntrinsicInst *ii = dyn_cast<IntrinsicInst>(inst))
    intrinsicInstList.push_back(ii);
}

pdg::ArgumentWrapper *pdg::FunctionWrapper::getArgWByIdx(int idx)
{
  if (idx > argWList.size())
  {
    errs() << "request index excess argW list length... Return nullptr"
           << "\n";
    return nullptr;
  }
  return argWList[idx];
}