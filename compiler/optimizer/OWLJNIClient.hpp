//
// created by cijie xia on 2019-09-10.
// An high-level abstraction of Java Native Interface
//

#ifndef omr_owljniclient_h
#define omr_owljniclient_h

#include <stdint.h>
#include "launch.h"

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
    TR_OWLJNIClient();
    static JNIEnv *_env;
    static TR_OWLJNIClient *_instance;
    jclass _getClass(const char* className);
    jmethodID _getMethodID(bool isStaticMethod, jclass cls, const char* methodName, const char* methodSig);
    jfieldID _getFieldId(bool isStaticField, jclass cls, const char* fieldName, const char* fieldSig);

public:
    
    static TR_OWLJNIClient *getInstance();
    static void destroyInstance();
    jstring constructString(char* str);

    jobject constructObject(int32_t i);
    jobject constructObject(float i);
    jobject constructObject(double i);
    jobject constructObject(int16_t i);
    jobject constructObject(int64_t i);

    /* Field */
    void getField(FieldConfig fieldConfig, jobject obj, jobject*res);

    void callMethod(MethodConfig methodConfig, jobject obj, jobject* res, uint16_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, int32_t* res, uint16_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, int64_t* res, uint16_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, int16_t* res, uint16_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, float* res, uint16_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, double* res, uint16_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, char* res, uint16_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, bool* res, uint16_t argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, char** res, uint16_t argNum, ...);

};
#endif //omr_OWLJNIClient_h
