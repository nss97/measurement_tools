#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <string>
#include "pin.H"

std::ofstream TraceFile;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "output.log", "specify trace file name");

typedef struct {
    bool Taken;
    ADDRINT predTarget;
} BP_Info;

class BranchPredictor {
private:

    map<ADDRINT, ADDRINT> TargetTable;
    map<ADDRINT, int> BPState;

    void UpdateState(bool BrTaken, ADDRINT PC) {
        if (BrTaken)
            BPState[PC]--;
        if (!BrTaken)
            BPState[PC]++;

        if (BPState[PC] > 3)
            BPState[PC] = 3;
        if (BPState[PC] < 0)
            BPState[PC] = 0;
    }

    bool GetState(ADDRINT PC) {
        if (BPState[PC] == 0)
            return true;
        if (BPState[PC] == 1)
            return true;
        if (BPState[PC] == 2)
            return false;
        if (BPState[PC] == 3)
            return false;
        return true;
    }


public:
    BP_Info GetPrediction(ADDRINT PC) {
        BP_Info tmp_info;

        if ( TargetTable.find(PC) == TargetTable.end() ) {
            //not found
            tmp_info.Taken = true;
            tmp_info.predTarget = -1;
            // taken default
        } else {
            tmp_info.Taken = GetState(PC);
            tmp_info.predTarget = TargetTable[PC];
        }

        return tmp_info;
    }

    void Update(ADDRINT PC, bool BrTaken, ADDRINT targetPC) {
        // if not equal & exist before
//      if ( TargetTable[PC] != targetPC && TargetTable.find(PC) != TargetTable.end() )
//	    cout << "Target Changed" << endl;

        if ( BPState.find(PC) == BPState.end()) {
            // init
            BPState[PC] = 0;
        }

        UpdateState(BrTaken, PC);
	    TargetTable[PC] = targetPC;
    }
    
    int GetSizeOfBP() {
        return TargetTable.size();
    }
};

BranchPredictor myBPU;

long long int DirectionMissCount = 0;
long long int TargetMissCount = 0;
long long int BranchCount = 0;

VOID ProcessBranch(ADDRINT PC, ADDRINT targetPC, bool BrTaken) {
    // cout << PC << "\t" << targetPC << "\t" << BrTaken << endl;
    
    BranchCount++;
    BP_Info pred = myBPU.GetPrediction(PC);
    if ( pred.Taken != BrTaken) {
        DirectionMissCount++;
    }
    if ( pred.predTarget != targetPC ) {
        TargetMissCount++;
    }
    myBPU.Update(PC, BrTaken, targetPC);
}
    
VOID Instruction(INS ins, VOID *v)
{
    if (INS_IsBranch(ins) && INS_HasFallThrough(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) ProcessBranch, 
	IARG_ADDRINT, INS_Address(ins), 
	IARG_ADDRINT, INS_DirectBranchOrCallTargetAddress(ins),
	IARG_BRANCH_TAKEN, IARG_END);
}


VOID Fini(INT32 code, VOID *v)
{
    TraceFile << "################################################" << endl;
    TraceFile << "DirectionMissCount: " << DirectionMissCount << endl;
    TraceFile << "TargetMissCount: " << TargetMissCount << endl;
    TraceFile << "BranchCount: " << BranchCount << endl;
    TraceFile << "Branch Direction Miss rate: " << (DirectionMissCount / (float)BranchCount) * 100 << "%" << endl;
    TraceFile << "Branch Target Miss rate: " << (TargetMissCount / (float)BranchCount) * 100 << "%" << endl;
    TraceFile << "################################################" << endl;
}

int main(int argc, char * argv[])
{
    TraceFile.open(KnobOutputFile.Value().c_str());
    TraceFile.setf(ios::showbase);

    PIN_Init(argc, argv);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    
    return 0;
}
