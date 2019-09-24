//
// Created by Cijie Xia on 2019-09-10.
//

#include "OWLJNIClient.hpp"
#include <cstdlib>
#include <iostream>
#include <string.h>

/* ================= private ================= */
TR_OWLJNIClient* TR_OWLJNIClient::_instance = NULL;
JNIEnv * TR_OWLJNIClient::_env = NULL;

TR_OWLJNIClient::TR_OWLJNIClient() {
    //start jvm
    char *walaHome = std::getenv("WALA_HOME");
    char classpath[1024];
    sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes", walaHome, walaHome, walaHome, walaHome);
    _env = launch_jvm(classpath);
    std::cout << "Successfully launch JVM!" << std::endl;
}

TR_OWLJNIClient::~TR_OWLJNIClient() {}

jclass TR_OWLJNIClient::_getClass(const char *className) {
    jclass cls = _env->FindClass(className);
    if (_env->ExceptionCheck()){
        std::cout<< "Fail to find class "<< className << std::endl;
        exit(1);
    }
    return cls;
}

jmethodID TR_OWLJNIClient::_getMethodID(bool isStaticMethod, jclass cls, const char *methodName, const char *methodSig) {
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

jfieldID TR_OWLJNIClient::_getFieldId(bool isStaticField, jclass cls, const char *fieldName, const char *fieldSig) {
    jfieldID fid;
    if (isStaticField){
        fid = _env->GetStaticFieldID(cls, fieldName, fieldSig);
    }
    else{
        fid = _env->GetFieldID(cls, fieldName, fieldSig);
    }

    if (_env->ExceptionCheck()) {
        std::cout<< "Fail to find field "<< fieldName << std::endl;
        exit(1);
    }
    return fid;
}

/* ================= public ======================= */
TR_OWLJNIClient* TR_OWLJNIClient::getInstance() {
    if (!_instance){
        _instance = new TR_OWLJNIClient;
    }
    return _instance;
}

jstring TR_OWLJNIClient::constructString(char *str) {
    return _env->NewStringUTF(str);
}

jobject TR_OWLJNIClient::constructIntObject(int i){
    jclass cls = _getClass("java/lang/Integer");
    jmethodID mid = _getMethodID(false, cls, "<init>", "(I)V");
    jobject intObject = _env->NewObject(cls, mid, i);
    return intObject;
}

/* Field */

void TR_OWLJNIClient::getField(FieldConfig fieldConfig, jobject obj, jobject *res) {
    jclass cls = _getClass(fieldConfig.className);
    jfieldID fid = _getFieldId(fieldConfig.isStatic, cls, fieldConfig.fieldName, fieldConfig.fieldSig);
    if (fieldConfig.isStatic){
        *res = _env->GetStaticObjectField(cls,fid);
    }
    else{
        *res = _env->GetObjectField(obj,fid);
    }
}

/* static */

//Void
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    _env->CallStaticVoidMethodV(cls,mid,args);
    va_end(args);
}

// Object
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, jobject *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticObjectMethodV(cls,mid,args);
    va_end(args);
}

// int
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, int *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticIntMethodV(cls,mid,args);
    va_end(args);
}

// long
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, long *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticLongMethodV(cls,mid,args);
    va_end(args);
}

// short
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, short *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticShortMethodV(cls,mid,args);
    va_end(args);
}

// float
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, float *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticFloatMethodV(cls,mid,args);
    va_end(args);
}

// double
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, double *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticDoubleMethodV(cls,mid,args);
    va_end(args);
}

// char
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, char *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticCharMethodV(cls,mid,args);
    va_end(args);
}

// char* (string)
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, char **res, int argNum, ...) {

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
void TR_OWLJNIClient::callStaticMethod(MethodConfig methodConfig, bool *res, int argNum, ...) {

    va_list args;
    va_start(args,argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(true, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallStaticBooleanMethodV(cls,mid,args);
    va_end(args);
}


/* non-static */

//void
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    _env->CallVoidMethodV(obj,mid,args);
    va_end(args);
}

//object
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, jobject* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallObjectMethodV(obj,mid,args);
    va_end(args);
}

//int
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, int* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallIntMethodV(obj,mid,args);
    va_end(args);
}

//long
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, long* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallLongMethodV(obj,mid,args);
    va_end(args);
}

//short
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, short* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallShortMethodV(obj,mid,args);
    va_end(args);
}

//float
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, float* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallFloatMethodV(obj,mid,args);
    va_end(args);
}

//double
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, double* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallDoubleMethodV(obj,mid,args);
    va_end(args);
}

//char
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, char* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallCharMethodV(obj,mid,args);
    va_end(args);
}

//bool
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, bool* res, int argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(false, cls, methodConfig.methodName, methodConfig.methodSig);
    *res = _env->CallBooleanMethodV(obj,mid,args);
    va_end(args);
}

// char* (string)
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, char **res, int argNum, ...) {

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


