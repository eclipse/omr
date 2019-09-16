//
// Created by Cijie Xia on 2019-09-10.
//

#include "JNIClient.hpp"
#include <cstdlib>
#include <iostream>
#include <string.h>

/* ================= private ================= */
TR_JNIClient* TR_JNIClient::_instance = NULL;
JNIEnv * TR_JNIClient::_env = NULL;

TR_JNIClient::TR_JNIClient() {
    //start jvm
    char *walaHome = std::getenv("WALA_HOME");
    char classpath[1024];
    sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes", walaHome, walaHome, walaHome, walaHome);
    _env = launch_jvm(classpath);
    std::cout << "Successfully launch JVM!" << std::endl;
}



jclass TR_JNIClient::_getClass(const char *className) {
    jclass cls = _env->FindClass(className);
    if (_env->ExceptionCheck()){
        std::cout<< "Fail to find class "<< className << std::endl;
        exit(1);
    }
    return cls;
}

jmethodID TR_JNIClient::_getMethodID(bool isStaticMethod, jclass cls, const char *methodName, const char *methodSig) {
    jmethodID mid;
    if (isStaticMethod){
        mid = _env->GetStaticMethodID(cls,methodName,methodSig);
    }
    else{
        mid = _env->GetMethodID(cls,methodName,methodSig);
    }

    if (_env->ExceptionCheck()){
        std::cout<< "Fail to find method "<< methodName << std::endl;
        exit(1);
    }
    return mid;
}

/* ================= public ======================= */
TR_JNIClient* TR_JNIClient::getInstance() {
    if (!_instance){
        _instance = new TR_JNIClient;
    }
    return _instance;
}

/* static methods */

//Void
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    _env->CallStaticVoidMethodV(cls,mid,args);
    va_end(args);
}

// Object
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, jobject *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticObjectMethodV(cls,mid,args);
    va_end(args);
}

// int
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, int *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticIntMethodV(cls,mid,args);
    va_end(args);
}

// long
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, long *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticLongMethodV(cls,mid,args);
    va_end(args);
}

// short
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, short *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticShortMethodV(cls,mid,args);
    va_end(args);
}

// float
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, float *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticFloatMethodV(cls,mid,args);
    va_end(args);
}

// double
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, double *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticDoubleMethodV(cls,mid,args);
    va_end(args);
}

// char
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, char *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticCharMethodV(cls,mid,args);
    va_end(args);
}

// char* (string)
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, char **res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    jstring out = (jstring)(_env->CallStaticObjectMethodV(cls,mid,args));
    va_end(args);
    char temp[1024];
    sprintf(temp,_env->GetStringUTFChars(out,0));
    *res = temp;
}

// bool
void TR_JNIClient::callStaticMethod(MethodConfig methodConfig, bool *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticBooleanMethodV(cls,mid,args);
    va_end(args);
}


/* non-static methods */

//void
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    _env->CallVoidMethodV(obj,mid,args);
    va_end(args);
}

//object
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, jobject* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallObjectMethodV(obj,mid,args);
    va_end(args);
}

//int
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, int* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallIntMethodV(obj,mid,args);
    va_end(args);
}

//long
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, long* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallLongMethodV(obj,mid,args);
    va_end(args);
}

//short
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, short* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallShortMethodV(obj,mid,args);
    va_end(args);
}

//float
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, float* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallFloatMethodV(obj,mid,args);
    va_end(args);
}

//double
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, double* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallDoubleMethodV(obj,mid,args);
    va_end(args);
}

//char
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, char* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallCharMethodV(obj,mid,args);
    va_end(args);
}

//bool
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, bool* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallBooleanMethodV(obj,mid,args);
    va_end(args);
}

// char* (string)
void TR_JNIClient::callMethod(MethodConfig methodConfig, jobject obj, char **res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    jstring out = (jstring)(_env->CallObjectMethodV(obj,mid,args));
    va_end(args);
    char temp[1024];
    sprintf(temp,_env->GetStringUTFChars(out,0));
    *res = temp;

}

TR_JNIClient::~TR_JNIClient() {}
