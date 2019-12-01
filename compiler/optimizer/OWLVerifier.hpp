#ifndef OWL_VERIFIER_HPP
#define OWL_VERIFIER_HPP

#include "optimizer/OWLJNIClient.hpp"

class TR_OWLVerifier
{
private:
    TR_OWLJNIClient* _jniClient; 
    jobject verifier; // the actual java verifier
public:
    TR_OWLVerifier(TR_OWLJNIClient* jniClient);
    bool verify(jobject* shrikeBTInstructions, uint64_t size, bool isConstructor, bool isStatic ,char* classType, char* signature);
    
};


#endif