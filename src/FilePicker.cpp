#include "FilePicker.h"

#if defined(__ANDROID__)
#include <jni.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>

#define LOG_TAG "FilePicker"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Get the android_app from Raylib
extern "C" struct android_app* GetAndroidApp();

// Store class loader and class reference at init time
static jclass g_blackLangAppClass = nullptr;
static jmethodID g_openFilePickerMethod = nullptr;

struct AndroidInputEvent {
    int key;
    int charCode;
};
static std::queue<AndroidInputEvent> g_inputQueue;
static std::mutex g_inputMutex;

namespace {
    std::mutex g_mutex;
    std::condition_variable g_cv;
    std::string g_pickedFilePath;
    bool g_filePickerComplete = false;
}

namespace FilePicker {

void setPickedFilePath(const char* path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pickedFilePath = path ? path : "";
    g_filePickerComplete = true;
    g_cv.notify_all();
    LOGD("File picked: %s", g_pickedFilePath.c_str());
}

bool isFilePickerPending() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return !g_filePickerComplete && !g_pickedFilePath.empty();
}

std::string getPickedFilePath() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_pickedFilePath;
}

void clearPickedFilePath() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pickedFilePath.clear();
    g_filePickerComplete = false;
}

static std::string launchFilePicker(const char* mimeType) {
    if (!g_blackLangAppClass || !g_openFilePickerMethod) {
        LOGD("FilePicker not initialized - call initFilePicker first");
        return "";
    }
    
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) {
        LOGD("Android app not available");
        return "";
    }
    
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);
    if (!env) {
        LOGD("Failed to attach thread");
        return "";
    }
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pickedFilePath.clear();
        g_filePickerComplete = false;
    }
    
    jstring jMimeType = env->NewStringUTF(mimeType);
    env->CallStaticVoidMethod(g_blackLangAppClass, g_openFilePickerMethod, jMimeType);
    env->DeleteLocalRef(jMimeType);
    
    if (env->ExceptionCheck()) {
        LOGD("Exception during openFilePicker call");
        env->ExceptionDescribe();
        env->ExceptionClear();
        return "";
    }
    
    LOGD("File picker launched, waiting for result...");
    
    std::unique_lock<std::mutex> lock(g_mutex);
    if (g_cv.wait_for(lock, std::chrono::seconds(120), []{ return g_filePickerComplete; })) {
        return g_pickedFilePath;
    }
    
    LOGD("File picker timeout");
    return "";
}

std::string openAudioFile() {
    return launchFilePicker("audio/*");
}

std::string openProjectFile() {
    return launchFilePicker("application/json");
}

std::string saveProjectFile() {
    return launchFilePicker("application/json"); // TODO: save dialog
}

void requestPermissions() {
    LOGE("requestPermissions called");
    if (!g_blackLangAppClass) return;
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) return;
    
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);
    if (!env) return;
    
    jmethodID method = env->GetStaticMethodID(g_blackLangAppClass, "requestStoragePermission", "()V");
    if (method) env->CallStaticVoidMethod(g_blackLangAppClass, method);
    
    app->activity->vm->DetachCurrentThread();
}

std::string getWritablePath() {
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity) return "";
    return std::string(app->activity->internalDataPath) + "/";
}

void exportFile(const std::string& sourcePath, const std::string& targetName) {
    if (!g_blackLangAppClass) return;
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) return;
    
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);
    if (!env) return;
    
    jmethodID method = env->GetStaticMethodID(g_blackLangAppClass, "exportFile", "(Ljava/lang/String;)V");
    if (method) {
        jstring jPath = env->NewStringUTF(sourcePath.c_str());
        env->CallStaticVoidMethod(g_blackLangAppClass, method, jPath);
        env->DeleteLocalRef(jPath);
    }
    
    app->activity->vm->DetachCurrentThread();
}

bool hasPermissions() { return true; }

void showKeyboard() {
    LOGE("showKeyboard called");
    if (!g_blackLangAppClass) {
         LOGE("g_blackLangAppClass is null");
         return;
    }
    
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) {
        LOGE("Android app struct invalid");
        return;
    }
    
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);
    if (!env) {
        LOGE("Failed to connect JNI");
        return;
    }
    
    jmethodID method = env->GetStaticMethodID(g_blackLangAppClass, "showKeyboard", "()V");
    if (method) {
        env->CallStaticVoidMethod(g_blackLangAppClass, method);
    } else {
        LOGE("showKeyboard method not found");
        env->ExceptionClear();
    }
    app->activity->vm->DetachCurrentThread();
}

void hideKeyboard() {
    if (!g_blackLangAppClass) return;
    struct android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) return;
    
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);
    if (env) {
        jmethodID method = env->GetStaticMethodID(g_blackLangAppClass, "hideKeyboard", "()V");
        if (method) env->CallStaticVoidMethod(g_blackLangAppClass, method);
        app->activity->vm->DetachCurrentThread();
    }
}

bool AndroidGetInput(int& key, int& charCode) {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    if (g_inputQueue.empty()) return false;
    AndroidInputEvent e = g_inputQueue.front();
    g_inputQueue.pop();
    key = e.key;
    charCode = e.charCode;
    return true;
}

} // namespace FilePicker

extern "C" JNIEXPORT void JNICALL
Java_com_quadracollision_blacklang_BlackLangApplication_initFilePicker(
    JNIEnv* env, jclass clazz) {
    if (g_blackLangAppClass) {
        env->DeleteGlobalRef(g_blackLangAppClass);
    }
    g_blackLangAppClass = (jclass)env->NewGlobalRef(clazz);
    
    // Also cache openFilePickerMethod for file picking
    g_openFilePickerMethod = env->GetStaticMethodID(clazz, "openFilePicker", "(Ljava/lang/String;)V");
    
    LOGE("FilePicker initialized. Class: %p", g_blackLangAppClass);
}

extern "C" JNIEXPORT void JNICALL
Java_com_quadracollision_blacklang_BlackLangApplication_nativeOnInput(
    JNIEnv* env, jclass clazz, jint key, jint charCode) {
    LOGE("nativeOnInput: k=%d c=%d", key, charCode);
    std::lock_guard<std::mutex> lock(g_inputMutex);
    g_inputQueue.push({(int)key, (int)charCode});
}

extern "C" JNIEXPORT void JNICALL
Java_com_quadracollision_blacklang_BlackLangApplication_nativeOnFilePicked(
    JNIEnv* env, jclass clazz, jstring path) {
    const char* p = path ? env->GetStringUTFChars(path, nullptr) : nullptr;
    FilePicker::setPickedFilePath(p);
    if (p) env->ReleaseStringUTFChars(path, p);
}

#else // Desktop implementation

#include "tinyfiledialogs.h"
#include <filesystem>

namespace FilePicker {

std::string openAudioFile() {
    const char* filterPatterns[3] = {"*.wav", "*.mp3", "*.ogg"};
    const char* filePath = tinyfd_openFileDialog(
        "Open Sample",
        "./",
        3,
        filterPatterns,
        "Audio Files",
        0
    );
    return filePath ? filePath : "";
}

std::string openProjectFile() {
    const char* filters[1] = {"*.json"};
    const char* path = tinyfd_openFileDialog("Load Project", "", 1, filters, "JSON Project Files", 0);
    return path ? path : "";
}

std::string saveProjectFile() {
    const char* filters[1] = {"*.json"};
    const char* path = tinyfd_saveFileDialog("Save Project", "project.json", 1, filters, "JSON Project Files");
    return path ? path : "";
}

std::string getWritablePath() {
    // Current directory + recordings
    if (!std::filesystem::exists("recordings")) {
        std::filesystem::create_directory("recordings");
    }
    return "recordings/";
}

void exportFile(const std::string& sourcePath, const std::string& targetName) {
    // On Desktop, "Export" effectively means "Save As" / Move
    // Or just open the folder :)
    // Let's implement a Save As dialog to move the file
    
    std::string defaultName = targetName.empty() ? "recording.wav" : targetName;
    const char* filters[1] = {"*.wav"};
    const char* destPath = tinyfd_saveFileDialog("Save Recording", defaultName.c_str(), 1, filters, "WAV Files");
    
    if (destPath) {
        try {
            if (std::filesystem::exists(destPath)) {
                std::filesystem::remove(destPath);
            }
            std::filesystem::copy_file(sourcePath, destPath);
        } catch (std::exception& e) {
            // log error
        }
    }
}

void requestPermissions() {}
bool hasPermissions() { return true; }
void showKeyboard() {}
void hideKeyboard() {}
bool AndroidGetInput(int& key, int& charCode) { return false; }

} // namespace FilePicker

#endif
