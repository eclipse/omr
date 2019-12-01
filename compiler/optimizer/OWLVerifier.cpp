#include "optimizer/OWLVerifier.hpp"
#include "optimizer/OWLJNIConfig.hpp"


TR_OWLVerifier::TR_OWLVerifier(TR_OWLJNIClient* jniClient) {
    _jniClient = jniClient;
}

bool TR_OWLVerifier::verify(jobject* shrikeBTInstructions, uint64_t size, bool isConstructor, bool isStatic ,char* classType, char* signature) {
    jobjectArray instructions = _jniClient->newObjectArray(InstructionClassName,shrikeBTInstructions, size);

    int instToBC[size];

    for (uint64_t i = 0 ; i < size ; i++ ) {
        instToBC[i] = i;
    }
    
    jobjectArray handlers[size];

    for (uint64_t i = 0 ; i < size; i ++) {
        handlers[i] = _jniClient->newObjectArray(ExceptionHandlerClassName, NULL, 0);
    }

    jobjectArray vars = nullptr;

    
    jobject verifier = _jniClient->newObject
    (
        Verifier,
        8,
        isConstructor ? JNI_TRUE: JNI_FALSE,
        isStatic ? JNI_TRUE : JNI_FALSE,
        _jniClient->newString(classType),
        _jniClient->newString(signature),
        instructions,
        _jniClient->newMultidimentionalObjectArray(handlers, size), 
        _jniClient->newIntegerArray(instToBC, size),
        vars
    );

    bool status = _jniClient->callMethod(Verifier_verify_Config, verifier, 0);

    return status;
}



