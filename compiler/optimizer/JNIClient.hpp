//
// created by cijie xia on 2019-09-10.
//

#ifndef omr_jniclient_h
#define omr_jniclient_h

#include "launch.h"

// singleton pattern
class TR_JNIClient
{
private:
    TR_JNIClient();
    static JNIEnv *env;
    static TR_JNIClient *instance;

public:
    ~TR_JNIClient();
    static TR_JNIClient *getInstance();
    jclass getClass(char* className);
};
#endif //omr_jniclient_h
