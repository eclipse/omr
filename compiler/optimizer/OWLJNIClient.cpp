//
// Created by Cijie Xia on 2019-09-10.
//

#include <cstdlib>
#include <string.h>
#include <stdio.h>
#include "optimizer/OWLJNIClient.hpp"

/* ================= private ================= */
TR_OWLJNIClient* TR_OWLJNIClient::_instance = NULL;
JNIEnv * TR_OWLJNIClient::_env = NULL;

TR_OWLJNIClient::TR_OWLJNIClient() {
    //start jvm
    char *walaHome = std::getenv("WALA_HOME");
    char classpath[1024];
    sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes", walaHome, walaHome, walaHome, walaHome);
    _env = launch_jvm(classpath);
    printf("=============Successfully launch JVM!=============\n");
}

jclass TR_OWLJNIClient::_getClass(const char *className) {
    jclass cls = _env->FindClass(className);
    if (_env->ExceptionCheck()){
        printf("Fail to find class %s\n",className);
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
        printf("Fail to find method %s\n",methodName);
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
        printf("Fail to find field %s\n",fieldName);
        exit(1);
    }
    return fid;
}

/*** public ***/
TR_OWLJNIClient* TR_OWLJNIClient::getInstance() {
    if (!_instance){
        _instance = new TR_OWLJNIClient;
    }
    return _instance;
}

void TR_OWLJNIClient::destroyInstance() {}

jstring TR_OWLJNIClient::constructString(char *str) {
    return _env->NewStringUTF(str);
}

jobject TR_OWLJNIClient::constructObject(int32_t i){
    jclass cls = _getClass("java/lang/Integer");
    jmethodID mid = _getMethodID(false, cls, "<init>", "(I)V");
    jobject integerObject = _env->NewObject(cls, mid, i);
    return integerObject;
}

jobject TR_OWLJNIClient::constructObject(float i){
    jclass cls = _getClass("java/lang/Float");
    jmethodID mid = _getMethodID(false, cls, "<init>", "(F)V");
    jobject floatObject = _env->NewObject(cls, mid, i);
    return floatObject;
}

jobject TR_OWLJNIClient::constructObject(double i){
    jclass cls = _getClass("java/lang/Double");
    jmethodID mid = _getMethodID(false, cls, "<init>", "(D)V");
    jobject doubleObject = _env->NewObject(cls, mid, i);
    return doubleObject;
}

jobject TR_OWLJNIClient::constructObject(int16_t i) {
    jclass cls = _getClass("java/lang/Short");
    jmethodID mid = _getMethodID(false, cls, "<init>", "(S)V");
    jobject shortObject = _env->NewObject(cls, mid, i);
    return shortObject;
}

jobject TR_OWLJNIClient::constructObject(int64_t i) {
    jclass cls = _getClass("java/lang/Long");
    jmethodID mid = _getMethodID(false, cls, "<init>", "(J)V");
    jobject longObject = _env->NewObject(cls,mid,i);
    return longObject;
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


//object
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, jobject* res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.className, cls, methodConfig.methodName, methodConfig.methodSig);
    if (methodConfig.isStatic){
        *res = _env->CallStaticObjectMethodV(cls, mid, args);
    }
    else{
        *res = _env->CallObjectMethodV(obj,mid,args);
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);
}

//int
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, int32_t* res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.isStatic, cls, methodConfig.methodName, methodConfig.methodSig);
    if (methodConfig.isStatic){
        *res = _env->CallStaticIntMethodV(cls,mid,args);
    }
    else{
        *res = _env->CallIntMethodV(obj,mid,args);
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);

}

//long
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, int64_t* res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.isStatic, cls, methodConfig.methodName, methodConfig.methodSig);
    if (methodConfig.isStatic){
        *res = _env->CallStaticLongMethodV(cls, mid, args);
    }
    else{
        *res = _env->CallLongMethodV(obj,mid,args);
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);
}

//short
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, int16_t* res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.isStatic, cls, methodConfig.methodName, methodConfig.methodSig);
    if (methodConfig.isStatic){
        *res = _env->CallStaticShortMethodV(cls, mid, args);
    }
    else{
        *res = _env->CallShortMethodV(obj,mid,args);
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);
}

//float
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, float* res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.isStatic, cls, methodConfig.methodName, methodConfig.methodSig);
    if (methodConfig.isStatic) {
        *res = _env->CallStaticFloatMethodV(cls,mid,args);
    }
    else{
        *res = _env->CallFloatMethodV(obj,mid,args);
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);
}

//double
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, double* res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.isStatic, cls, methodConfig.methodName, methodConfig.methodSig);
    if (methodConfig.isStatic){
        *res = _env->CallStaticDoubleMethodV(cls, mid, args);
    }
    else{
        *res = _env->CallDoubleMethodV(obj,mid,args);
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);
}

//char
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, char* res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.isStatic, cls, methodConfig.methodName, methodConfig.methodSig);
    if (methodConfig.isStatic){
        *res = _env->CallStaticCharMethodV(cls,mid,args);
    }
    else{
        *res = _env->CallCharMethodV(obj,mid,args);
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);
}

//bool
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, bool* res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.isStatic, cls, methodConfig.methodName, methodConfig.methodSig);
    if (methodConfig.isStatic){
        *res = _env->CallStaticBooleanMethodV(cls,mid,args);
    }
    else{
        *res = _env->CallBooleanMethodV(obj,mid,args);
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);
}

// char* (string)
void TR_OWLJNIClient::callMethod(MethodConfig methodConfig, jobject obj, char **res, uint16_t argNum, ...) {

    va_list args;
    va_start(args, argNum);
    jclass cls = _getClass(methodConfig.className);
    jmethodID mid = _getMethodID(methodConfig.isStatic, cls, methodConfig.methodName, methodConfig.methodSig);
    jstring out;
    if (methodConfig.isStatic){
        out  = (jstring)(_env->CallStaticObjectMethodV(cls,mid,args));
    }
    else{
        out = (jstring)(_env->CallObjectMethodV(obj,mid,args));
    }

    if (_env->ExceptionCheck()) {
        printf("Fail to call Method %s in %s\n",methodConfig.methodName, methodConfig.className);
        exit(1);
    }

    va_end(args);
    const char *str = _env->GetStringUTFChars(out,0);
    sprintf(*res,str);
    _env->ReleaseStringUTFChars(out,0);

}


