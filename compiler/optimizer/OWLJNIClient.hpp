//
// created by cijie xia on 2019-09-10.
// An high-level abstraction of Java Native Interface
//

#ifndef omr_owljniclient_h
#define omr_owljniclient_h

#include <stdint.h>
#include <vector>
#include "jni.h"

typedef struct JNIMethodConfig {
    bool isStatic;
    char* className;
    char* methodName;
    char* methodSig;
} JNIMethodConfig;

typedef struct JNIFieldConfig {
    bool isStatic;
    char* className;
    char* fieldName;
    char* fieldSig;
} JNIFieldConfig;

typedef struct JNIConstructorConfig {
    char* className;
    char* constructorSig; 
} JNIConstructorConfig;



/***
 * JNIClient uses singleton pattern.
 * Since only one JVM can be started in a process.
 * Even if JVM is destroyed, the JVM cannot be started again after a previous JVM was started.
 **/
class TR_OWLJNIClient
{
private:

    static JNIEnv *_env;
    static JavaVM *_jvm;
    static TR_OWLJNIClient * _instance;

    TR_OWLJNIClient();
    ~TR_OWLJNIClient();
    static bool _isJvmRunning;
    static bool _isJvmDestroyed;
    jclass _getClass(const char* className);
    jmethodID _getMethodID(bool isStaticMethod, jclass cls, const char* methodName, const char* methodSig);
    jfieldID _getFieldId(bool isStaticField, jclass cls, const char* fieldName, const char* fieldSig);

public:
    
    static TR_OWLJNIClient* getInstance();
    static void destroyInstance();

    static bool startJVM();
    static void destroyJVM();
    
    jstring newString(char* str);

    jobject newInteger(int32_t i);
    jobject newFloat(float i);
    jobject newDouble(double i);
    jobject newShort(int16_t i);
    jobject newLong(int64_t i);

    jobjectArray newObjectArray(const char* className, jobject* objects, uint64_t size);
    jobjectArray newMultidimentionalObjectArray(jobjectArray* innerArray, uint64_t size);

    jintArray newIntegerArray(int* array, int length);

    /* new object */
    jobject newObject(JNIConstructorConfig, int32_t argNum, ...);

    /* Field */
    void getField(JNIFieldConfig fieldConfig, jobject obj, jobject*res);

    /* invoke methods true-> invoke successful. false-> fail to invoke. Exception may generate*/
    bool callMethod(JNIMethodConfig methodConfig, jobject obj,               int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, jobject* res, int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, int32_t* res, int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, int64_t* res, int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, int16_t* res, int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, float* res, int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, double* res, int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, char* res, int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, bool* res, int32_t argNum, ...);
    bool callMethod(JNIMethodConfig methodConfig, jobject obj, char** res, int32_t argNum, ...);

};
#endif //omr_OWLJNIClient_h
