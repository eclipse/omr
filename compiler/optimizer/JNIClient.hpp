//
// created by cijie xia on 2019-09-10.
// An high-level abstraction of Java Native Interface
//

#ifndef omr_jniclient_h
#define omr_jniclient_h

#include "launch.h"

struct MethodConfig {
    char* className;
    char* methodName;
    char* methodSig;
};

// singleton pattern
class TR_JNIClient
{
private:
    TR_JNIClient();
    static JNIEnv *_env;
    static TR_JNIClient *_instance;
    jclass _getClass(const char* className);
    jmethodID _getMethodID(bool isStaticMethod, jclass cls, const char* methodName, const char* methodSig);

public:
    ~TR_JNIClient();
    static TR_JNIClient *getInstance();

    /*static*/
    void callStaticMethod(MethodConfig methodConfig,              int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, jobject* res, int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, int* res, int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, long* res, int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, short* res, int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, float* res, int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, double* res, int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, char* res, int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, char** res, int argNum, ...);
    void callStaticMethod(MethodConfig methodConfig, bool* res, int argNum, ...);

    /* non-static */
    void callMethod(MethodConfig methodConfig, jobject obj,              int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, jobject* res, int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, int* res, int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, long* res, int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, short* res, int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, float* res, int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, double* res, int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, char* res, int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, bool* res, int argNum, ...);
    void callMethod(MethodConfig methodConfig, jobject obj, char** res, int argNum, ...);

};
#endif //omr_jniclient_h
