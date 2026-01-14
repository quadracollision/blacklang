#include "AndroidBridge.h"

#if defined(__ANDROID__)
#include <jni.h>
#include <android/log.h>
#include <android_native_app_glue.h>

extern "C" struct android_app* GetAndroidApp();

#define LOG_TAG "AndroidBridge"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#include <iostream>
#define LOGD(...)
#endif

namespace platform {

#if defined(__ANDROID__)

// Helper to get environment safely
static JNIEnv* GetEnv(struct android_app* app, bool& attached) {
    if (!app || !app->activity || !app->activity->vm) return nullptr;
    
    JNIEnv* env = nullptr;
    jint res = app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    
    if (res == JNI_EDETACHED) {
        res = app->activity->vm->AttachCurrentThread(&env, NULL);
        if (res == JNI_OK) {
            attached = true;
        } else {
            return nullptr;
        }
    }
    
    return env;
}

void ShowToast(const std::string& message) {
    struct android_app* app = GetAndroidApp();
    if (!app) return;

    bool attached = false;
    JNIEnv* env = GetEnv(app, attached);
    if (!env) return;

    jobject activityInstance = app->activity->clazz;
    if (activityInstance) {
        jclass activityClass = env->GetObjectClass(activityInstance);
        if (activityClass) {
            jmethodID showToastMethod = env->GetMethodID(activityClass, "showToast", "(Ljava/lang/String;)V");
            if (showToastMethod) {
                jstring jMsg = env->NewStringUTF(message.c_str());
                env->CallVoidMethod(activityInstance, showToastMethod, jMsg);
                env->DeleteLocalRef(jMsg);
            } else {
                 env->ExceptionClear();
            }
            env->DeleteLocalRef(activityClass);
        }
    }

    if (attached) {
        app->activity->vm->DetachCurrentThread();
    }
}

void LaunchFileSaver(const std::string& sourcePath, const std::string& filename) {
    LOGD("LaunchFileSaver: %s", sourcePath.c_str());
    
    struct android_app* app = GetAndroidApp();
    if (!app) return;

    bool attached = false;
    JNIEnv* env = GetEnv(app, attached);
    if (!env) return;

    // app->activity->clazz is actually the Global Ref to the Activity INSTANCE
    jobject activityInstance = app->activity->clazz;
    
    if (activityInstance) {
        jclass activityClass = env->GetObjectClass(activityInstance);
        if (activityClass) {
            jmethodID saveMethod = env->GetMethodID(activityClass, "launchFileSaver", "(Ljava/lang/String;Ljava/lang/String;)V");
            if (saveMethod) {
                jstring jPath = env->NewStringUTF(sourcePath.c_str());
                jstring jName = env->NewStringUTF(filename.c_str());
                env->CallVoidMethod(activityInstance, saveMethod, jPath, jName);
                env->DeleteLocalRef(jPath);
                env->DeleteLocalRef(jName);
            } else {
                LOGD("Method launchFileSaver not found"); 
                env->ExceptionClear();
            }
            env->DeleteLocalRef(activityClass);
        } else {
            LOGD("Could not get class from activity instance");
        }
    } else {
        LOGD("Activity instance reference invalid");
    }

    if (attached) {
        app->activity->vm->DetachCurrentThread();
    }
}

#else

void ShowToast(const std::string& message) {
    // Desktop placeholder
    std::cout << "[TOAST]: " << message << std::endl;
}

void LaunchFileSaver(const std::string& sourcePath, const std::string& filename) {
    // Desktop placeholder
    std::cout << "[SAF]: Launching saver for " << filename << std::endl;
}

#endif

} // namespace platform
