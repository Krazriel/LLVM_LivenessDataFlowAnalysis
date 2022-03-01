#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"

#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
using namespace std;

using namespace llvm;

namespace {

// This method implements what the pass does
void visitor(Function &F){

    // Here goes what you want to do with a pass
    
		string func_name = "main";
	    errs() << "LivenessAnalysis: " << F.getName() << "\n";
	    
	    // Comment this line
        //if (F.getName() != func_name) return;

        /* maps to store sets */
        unordered_map<string, std::vector<string>> blockUEVar;
        unordered_map<string, std::vector<string>> blockVarKill;
        unordered_map<string, std::vector<string>> blockLiveOut;

        /* retrieve UEVar and VarKill set for each block */
        for (auto& basic_block : F)
        {
            string blockName = string(basic_block.getName());
            /* initialize VarKill and UEVar set */
            std::vector<string> VarKill;
            std::vector<string> UEVar;
           
            /* Retrieve UEVar and VarKill set */
            for (auto& inst : basic_block)
            {
                if(inst.getOpcode() == Instruction::Load){
                    string var = string(inst.getOperand(0)->getName());

                    /* if variable is not in VarKill and is already not in UEVar */
                    if(std::find(VarKill.begin(), VarKill.end(), var) == VarKill.end() && std::find(UEVar.begin(), UEVar.end(), var) == UEVar.end()){
                        UEVar.push_back(var);
                    }
                }
                if(inst.getOpcode() == Instruction::Store){

                    string var1 = "";
                    string var2 = "";

                    /* if first operand is a constant, ignore */
                    if(isa<ConstantInt>(inst.getOperand(0))){
                        var1 = "";
                    }

                    else{
                        /* if first operand is a binary op */
                        var1 = string(dyn_cast<User>(dyn_cast<User>(inst.getOperand(0))->getOperand(0))->getOperand(0)->getName());
                        var2 = string(dyn_cast<User>(dyn_cast<User>(inst.getOperand(0))->getOperand(1))->getOperand(0)->getName());

                        /* if first operand returns empty (implies a register or load instruction) */
                        if(string(inst.getOperand(0)->getName()) == ""){
                            var1 = string(dyn_cast<User>(inst.getOperand(0))->getOperand(0)->getName());
                        }

                        /* if var1 is a constant */
                        if(isa<ConstantInt>(dyn_cast<User>(dyn_cast<User>(inst.getOperand(0))->getOperand(0))->getOperand(0))){
                            var1 = "";
                        }

                        /* if var2 is a constant */
                        if(isa<ConstantInt>(dyn_cast<User>(dyn_cast<User>(inst.getOperand(0))->getOperand(1))->getOperand(0))){
                            var2 = "";
                        }
                    }

                    /* retrieve name of destination */
                    string varName = string(inst.getOperand(1)->getName());

                    /* if var1 is not in VarKill set and is also not already in UEVar set */
                    if(std::find(VarKill.begin(), VarKill.end(), var1) == VarKill.end() && std::find(UEVar.begin(), UEVar.end(), var1) == UEVar.end()){
                        UEVar.push_back(var1);
                    }

                    /* if var2 is not in VarKill set and is also not already in UEVar set */
                    if(std::find(VarKill.begin(), VarKill.end(), var2) == VarKill.end() && std::find(UEVar.begin(), UEVar.end(), var2) == UEVar.end()){
                        UEVar.push_back(var2);
                    }

                    /* if varName is not already in VarKill */
                    if(std::find(VarKill.begin(), VarKill.end(), varName) == VarKill.end()){
                        VarKill.push_back(varName);
                    }
                }
   
            } //end for inst

            /* Save sets to corresponding block */
            blockUEVar.insert(make_pair(blockName, UEVar));
            blockVarKill.insert(make_pair(blockName, VarKill));

        } // end for block

        /* initialize empty set for LiveOut */
        for (auto& basic_block : F){
            vector<string> emptySet = {};
            blockLiveOut.insert(make_pair(string(basic_block.getName()), emptySet));
        }

        /* Computation of LiveOut */
        unordered_map<string ,std::vector<string>>::const_iterator blockLookUp;
        bool cont = true;
        while(cont){
            cont = false;
            for (auto& basic_block : F){
                std::vector<string> liveOut;
                std::vector<string> liveOutTemp;
                std::vector<string> liveOutSucc;
                std::vector<string> varKillSucc;
                std::vector<string> ueVarSucc;
                std::vector<string> unionSuccessor;
          
                for(BasicBlock *Succ : successors(&basic_block)){
                    std::vector<string> diffTemp;
                    std::vector<string> unionTemp;

                    /* retrieve current sets of current block */
                    blockLookUp = blockLiveOut.find(string(Succ->getName()));
                    liveOutSucc = blockLookUp->second;
                    blockLookUp = blockVarKill.find(string(Succ->getName()));
                    varKillSucc = blockLookUp->second;
                    blockLookUp = blockUEVar.find(string(Succ->getName()));
                    ueVarSucc = blockLookUp->second;

                    /* LiveOut(X) - VarKill(X) */
                    std::set_difference(liveOutSucc.begin(), liveOutSucc.end(), varKillSucc.begin(), varKillSucc.end(), std::back_inserter(diffTemp));
                   
                    /* U UEVar(X) */
                    std::set_union(diffTemp.begin(), diffTemp.end(), ueVarSucc.begin(), ueVarSucc.end(), std::back_inserter(unionTemp));

                    for(auto it: unionTemp){
                        unionSuccessor.push_back(it);
                    }
                    
                }

                /* Compute Union of Successors */
                sort(unionSuccessor.begin(), unionSuccessor.end());
                unionSuccessor.erase(std::unique(unionSuccessor.begin(), unionSuccessor.end()), unionSuccessor.end());

                /* retrieve current LiveOut */
                blockLookUp = blockLiveOut.find(string(basic_block.getName()));
                liveOut = blockLookUp->second;

                /* If LiveOut(N) is changed */
                if(liveOut != unionSuccessor){
                    cont = true;
                }

                /* Update LiveOut */ 
                auto it = blockLiveOut.find(string(basic_block.getName()));
                it->second = unionSuccessor;
                
                
            }
        }

        /* Print out results */
        for (auto& basic_block : F){
            std::vector<string> UEVarTemp;
            std::vector<string> VarKillTemp;
            std::vector<string> LiveOutTemp;
            unordered_map<string ,std::vector<string>>::const_iterator blockLookUp;

            blockLookUp = blockUEVar.find(string(basic_block.getName()));
            UEVarTemp = blockLookUp->second;
            sort(UEVarTemp.begin(), UEVarTemp.end());
            errs() << "----- " << basic_block.getName() << " -----\n";
            errs() << "UEVAR: ";
            for(auto it: UEVarTemp){
                errs() << it << " ";
            }
            errs() << "\n";

                
            blockLookUp = blockVarKill.find(string(basic_block.getName()));
            VarKillTemp = blockLookUp->second;
            sort(VarKillTemp.begin(), VarKillTemp.end());
            errs() << "VARKILL: ";
            for(auto it: VarKillTemp){
                errs() << it << " ";
            }
            errs() << "\n";

            blockLookUp = blockLiveOut.find(string(basic_block.getName()));
            LiveOutTemp = blockLookUp->second;
            sort(LiveOutTemp.begin(), LiveOutTemp.end());
            errs() << "LIVEOUT: ";
            for(auto it: LiveOutTemp){
                errs() << it << " ";
            }
            errs() << "\n";
        }
        
        

        
}


// New PM implementation
struct LivenessAnalysisPass : public PassInfoMixin<LivenessAnalysisPass> {

  // The first argument of the run() function defines on what level
  // of granularity your pass will run (e.g. Module, Function).
  // The second argument is the corresponding AnalysisManager
  // (e.g ModuleAnalysisManager, FunctionAnalysisManager)
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
  	visitor(F);
	return PreservedAnalyses::all();

	
  }
  
    static bool isRequired() { return true; }
};
}



//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "LivenessAnalysisPass", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
        ArrayRef<PassBuilder::PipelineElement>) {
          if(Name == "liveness-analysis"){
            FPM.addPass(LivenessAnalysisPass());
            return true;
          }
          return false;
        });
    }};
}
