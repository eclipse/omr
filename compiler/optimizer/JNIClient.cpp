//
// Created by Cijie Xia on 2019-09-10.
//

#include "JNIClient.hpp"
#include <cstdlib>
#include <iostream>

TR_JNIClient* TR_JNIClient::instance = NULL;
JNIEnv * TR_JNIClient::env = NULL;

TR_JNIClient::TR_JNIClient() {
    //start jvm
    char *walaHome = std::getenv("WALA_HOME");
    char classpath[1024];
    sprintf(classpath, "%s/com.ibm.wala.util/target/classes:%s/com.ibm.wala.shrike/target/classes:%s/com.ibm.wala.core/target/classes:%s/com.ibm.wala.cast/target/classes", walaHome, walaHome, walaHome, walaHome);
    env = launch_jvm(classpath);
    std::cout << "Successfully launch JVM!" << std::endl;
}

TR_JNIClient* TR_JNIClient::getInstance() {
    if (!instance){
        instance = new TR_JNIClient;
    }
    return instance;
}

jclass TR_JNIClient::getClass(char *className) {
    jclass cls = env->FindClass(className);
    if (env->ExceptionCheck()){
        std::cout<< "Fail to find class "<< className << std::endl;
        exit(1);
    }
    return cls;
}

TR_JNIClient::~TR_JNIClient() {}
