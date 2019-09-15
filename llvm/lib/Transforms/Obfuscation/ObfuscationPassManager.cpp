#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Obfuscation/ObfuscationPassManager.h"
#include "llvm/Transforms/Obfuscation/ObfuscationOptions.h"
#include "llvm/Transforms/Obfuscation/IPObfuscationContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#define DEBUG_TYPE "ir-obfuscation"

using namespace llvm;

static cl::opt<bool>
    EnableIRObfusaction("irobf", cl::init(false), cl::NotHidden,
                        cl::desc("Enable IR Code Obfuscation."));
static cl::opt<bool>
    EnableIndirectBr("irobf-indbr", cl::init(false), cl::NotHidden,
                     cl::desc("Enable IR Indirect Branch Obfuscation."));

static cl::opt<std::string>
    GoronConfigure("goron-cfg", cl::desc("Goron configuration file"), cl::Optional);

namespace llvm {

struct ObfuscationPassManager : public ModulePass {
  static char ID; // Pass identification
  SmallVector<Pass *, 8> Passes;

  ObfuscationPassManager() : ModulePass(ID) {};

  StringRef getPassName() const override {
    return "Obfuscation Pass Manager";
  }

  bool doFinalization(Module &M) override {
    bool Change = false;
    for (Pass *P:Passes) {
      Change |= P->doFinalization(M);
      delete (P);
    }
    return Change;
  }

  void add(Pass *P) {
    Passes.push_back(P);
  }

  bool run(Module &M) {
    bool Change = false;
    for (Pass *P:Passes) {
      switch (P->getPassKind()) {
      case PassKind::PT_Function:Change |= runFunctionPass(M, (FunctionPass *) P);
        break;
      case PassKind::PT_Module:Change |= runModulePass(M, (ModulePass *) P);
        break;
      default:continue;
      }
    }
    return Change;
  }

  bool runFunctionPass(Module &M, FunctionPass *P) {
    bool Changed = false;
    for (Function &F : M) {
      Changed |= P->runOnFunction(F);
    }
    return Changed;
  }

  bool runModulePass(Module &M, ModulePass *P) {
    return P->runOnModule(M);
  }

  static ObfuscationOptions *getOptions() {
    ObfuscationOptions *Options = nullptr;
    if (sys::fs::exists(GoronConfigure.getValue())) {
      Options = new ObfuscationOptions(GoronConfigure.getValue());
    } else {
      SmallString<128> ConfigurePath;
      if (sys::path::home_directory(ConfigurePath)) {
        sys::path::append(ConfigurePath, "goron.yaml");
        Options = new ObfuscationOptions(ConfigurePath);
      } else {
        Options = new ObfuscationOptions();
      }
    }
    return Options;
  }

  bool runOnModule(Module &M) override {
    if (EnableIndirectBr) {
      EnableIRObfusaction = true;
    }

    if (!EnableIRObfusaction) {
      return false;
    }

    ObfuscationOptions *Options = getOptions();
    IPObfuscationContext *IPO = llvm::createIPObfuscationContextPass(true);

    add(IPO);
    add(llvm::createIndirectBranchPass(EnableIndirectBr | Options->EnableIndirectBr, IPO, Options));

    bool Changed = run(M);

    delete (Options);

    return Changed;
  }
};
}

char ObfuscationPassManager::ID = 0;
ModulePass *llvm::createObfuscationPassManager() { return new ObfuscationPassManager(); }
INITIALIZE_PASS(ObfuscationPassManager, "irobf", "Enable IR Obfuscation", false, false)
