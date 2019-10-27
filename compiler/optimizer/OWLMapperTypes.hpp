#ifndef OWL_MAPPER_TYPES
#define OWL_MAPPER_TYPES

#include "optimizer/OWLShrikeBTConstructor.hpp"
#include "il/OMRSymbolReference.hpp"
#include "launch.h"


/*** indicates how the offset of branch instruction should be adjusted ***/
enum BranchTargetLabelAdjustType {
    NO_ADJUST, // no need to adjust the branch target
    TABLE_MAP, // get the target label from the omr index to shrikeBT offset mapping table
    BY_VALUE, // inc or dec the offset by certain amount
};

struct InstructionMeta {
    /*** if false, this indicates it can be an OMR treetop, BBStart, BBEnd or an eliminated OMR instruction ***/
    bool isShrikeBTInstruction;
    uint32_t omrGlobalIndex;
    
    uint32_t shrikeBTOffset; // should be set to 0 initially and let the adjust function assign the correct offset
    BranchTargetLabelAdjustType branchTargetLabelAdjustType;
    int32_t branchTargetLabelAdjustAmount; // should be set to 0 if not using BY_VALUE adjust type
    ShrikeBTInstructionFieldsUnion instructionFieldsUnion;
    ShrikeBTInstruction instruction;
};

struct MappedInstructionObject {
    ShrikeBTInstruction instruction;
    jobject instructionObject;
    uint32_t offset;
};

#endif