//
// created by cijie xia on 2019-09-10.
// An high-level abstraction of Java Native Interface
//

#ifndef omr_owljniclient_h
#define omr_owljniclient_h

#include <stdint.h>
#include <vector>
#include "jni.h"

struct MethodConfig {
    bool isStatic;
    char* className;
    char* methodName;
    char* methodSig;
};

struct FieldConfig {
    bool isStatic;
    char* className;
    char* fieldName;
    char* fieldSig;
};

// singleton
class TR_OWLJNIClient
{
private:

    JNIEnv *_env;
    JavaVM *_jvm;
    bool _isJvmRunning;
    jclass _getClass(const char* className);
    jmethodID _getMethodID(bool isStaticMethod, jclass cls, const char* methodName, const char* methodSig);
    jfieldID _getFieldId(bool isStaticField, jclass cls, const char* fieldName, const char* fieldSig);

public:
    
    TR_OWLJNIClient();
    ~TR_OWLJNIClient();

    bool startJVM();
    
    jstring constructString(char* str);

    jobject constructObject(int32_t i);
    jobject constructObject(float i);
    jobject constructObject(double i);
    jobject constructObject(int16_t i);
    jobject constructObject(int64_t i);

    jobjectArray constructObjectArray(const char* className, std::vector<jobject> objects);

    /* Field */
    void getField(FieldConfig fieldConfig, jobject obj, jobject*res);

    void callMethod(MethodConfig methodConfig, jobject obj,               int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, jobject* res, int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, int32_t* res, int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, int64_t* res, int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, int16_t* res, int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, float* res, int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, double* res, int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, char* res, int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, bool* res, int32_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, char** res, int32_t argNum, ...);

};
#endif //omr_OWLJNIClient_h
