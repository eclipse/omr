#ifndef OWL_LOGGER_HPP
#define OWL_LOGGER_HPP

#include <vector>
#include "optimizer/OWLJNIClient.hpp"
#include "optimizer/OWLShrikeBTConstructor.hpp"
#include "optimizer/OWLMapperTypes.hpp"


class TR_OWLLogger
{

private:
    TR_OWLJNIClient *_jniClient;
public:
    TR_OWLLogger();
    ~TR_OWLLogger();

    char* getInstructionString(jobject instructionObj, ShrikeBTInstruction ins);
    void logSingleInstruction(jobject instructionObj, ShrikeBTInstruction instruction, uint32_t offset);
    void logAllMappedInstructions(std::vector<MappedInstructionObject> mappedInstructionObject); 
};

#endif