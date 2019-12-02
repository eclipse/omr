#include "optimizer/OWLAnalyser.hpp"
#include "optimizer/OWLJNIConfig.hpp"

TR_OWLAnalyser::TR_OWLAnalyser(TR_OWLJNIClient* jniClient){
    _jniClient = jniClient;
}

void TR_OWLAnalyser::analyse(jobject* instructions, uint64_t size) {
    jobjectArray instructionArray = _jniClient->newObjectArray(InstructionClassName, &instructions[0], size);
    _jniClient->callMethod(Printer_print_Config, NULL, 1, instructionArray);
}