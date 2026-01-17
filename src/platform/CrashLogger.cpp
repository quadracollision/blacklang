#include "CrashLogger.h"

#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(__ANDROID__)
#include <android/log.h>
#include <unwind.h>
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#define LOG_TAG "BlackLang-Crash"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...) 
#define LOGE(...)
#endif

namespace crash {

static std::string g_logPath;
static std::string g_downloadsPath;
static std::ofstream g_logFile;
static bool g_initialized = false;

// Ring buffer for recent log entries
static const int MAX_LOG_ENTRIES = 100;
static std::string g_logEntries[MAX_LOG_ENTRIES];
static int g_logIndex = 0;

// Get current timestamp string
static std::string getTimestamp() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "[%H:%M:%S]", t);
    return std::string(buf);
}

// Get date string for filename (mmddyy format)
static std::string getDateString() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[16];
    strftime(buf, sizeof(buf), "%m%d%y", t);
    return std::string(buf);
}

// Get full datetime for crash report header
static std::string getFullDateTime() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return std::string(buf);
}

#if defined(__ANDROID__)
// Stack unwinding
struct BacktraceState {
    void** current;
    void** end;
};

static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg) {
    BacktraceState* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        }
        *state->current++ = reinterpret_cast<void*>(pc);
    }
    return _URC_NO_REASON;
}

static size_t captureBacktrace(void** buffer, size_t max) {
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwindCallback, &state);
    return state.current - buffer;
}

static void writeStackTrace(FILE* file) {
    void* buffer[64];
    size_t count = captureBacktrace(buffer, 64);
    
    fprintf(file, "\n=== STACK TRACE ===\n");
    for (size_t i = 0; i < count; ++i) {
        Dl_info info;
        if (dladdr(buffer[i], &info) && info.dli_sname) {
            fprintf(file, "  #%02zu: %p %s + %ld (%s)\n", 
                    i, buffer[i], 
                    info.dli_sname,
                    (char*)buffer[i] - (char*)info.dli_saddr,
                    info.dli_fname ? info.dli_fname : "unknown");
        } else {
            fprintf(file, "  #%02zu: %p (unknown)\n", i, buffer[i]);
        }
    }
}

static const char* getSignalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (Segmentation Fault)";
        case SIGABRT: return "SIGABRT (Abort)";
        case SIGFPE:  return "SIGFPE (Floating Point Exception)";
        case SIGILL:  return "SIGILL (Illegal Instruction)";
        case SIGBUS:  return "SIGBUS (Bus Error)";
        default:      return "Unknown Signal";
    }
}

static void writeCrashReport(int sig, siginfo_t* info, const char* path) {
    FILE* file = fopen(path, "a");
    if (!file) return;
    
    fprintf(file, "\n\n========================================\n");
    fprintf(file, "CRASH REPORT - QC-33\n");
    fprintf(file, "========================================\n");
    fprintf(file, "Time: %s\n", getFullDateTime().c_str());
    fprintf(file, "Signal: %d - %s\n", sig, getSignalName(sig));
    if (info) {
        fprintf(file, "Fault address: %p\n", info->si_addr);
        fprintf(file, "Signal code: %d\n", info->si_code);
    }
    
    writeStackTrace(file);
    
    // Write recent log entries
    fprintf(file, "\n=== RECENT LOG ENTRIES ===\n");
    for (int i = 0; i < MAX_LOG_ENTRIES; ++i) {
        int idx = (g_logIndex + i) % MAX_LOG_ENTRIES;
        if (!g_logEntries[idx].empty()) {
            fprintf(file, "%s\n", g_logEntries[idx].c_str());
        }
    }
    fprintf(file, "========================================\n\n");
    
    fflush(file);
    int fd = fileno(file);
    if (fd != -1) {
        fsync(fd);
    }
    fclose(file);
}

static void signalHandler(int sig, siginfo_t* info, void* context) {
    (void)context;
    
    LOGE("CRASH! Signal %d: %s", sig, getSignalName(sig));
    
    // Write to app internal log
    if (!g_logPath.empty()) {
        writeCrashReport(sig, info, g_logPath.c_str());
        LOGE("Crash log written to: %s", g_logPath.c_str());
    }
    
    // Write to Downloads folder
    if (!g_downloadsPath.empty()) {
        writeCrashReport(sig, info, g_downloadsPath.c_str());
        LOGE("Crash log written to: %s", g_downloadsPath.c_str());
    }
    
    // Re-raise signal with default handler to let system handle it
    signal(sig, SIG_DFL);
    raise(sig);
}

static void setupSignalHandlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_SIGINFO;
    
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
}
#endif // __ANDROID__

void initCrashLogger() {
    if (g_initialized) return;
    
#if defined(__ANDROID__)
    // Get app internal files directory
    // We'll use a hardcoded path since we can't easily get the files dir in native code
    // The app data dir is typically /data/data/com.quadracollision.blacklang/files/
    const char* appDir = "/data/data/com.quadracollision.blacklang/files";
    
    // Create directory if needed
    mkdir(appDir, 0755);
    
    // Set up log paths
    std::string dateStr = getDateString();
    g_logPath = std::string(appDir) + "/qc33_crashlog_" + dateStr + ".log";
    
    // Downloads folder path
    g_downloadsPath = "/storage/emulated/0/Download/qc33_crashlog_" + dateStr + ".log";
    
    LOGD("CrashLogger initialized");
    LOGD("App log path: %s", g_logPath.c_str());
    LOGD("Downloads log path: %s", g_downloadsPath.c_str());
    
    // Write session header to app log
    FILE* f = fopen(g_logPath.c_str(), "a");
    if (f) {
        fprintf(f, "\n\n========================================\n");
        fprintf(f, "QC-33 Session Started\n");
        fprintf(f, "Time: %s\n", getFullDateTime().c_str());
        fprintf(f, "========================================\n");
        fclose(f);
    }
    
    setupSignalHandlers();
#endif
    
    g_initialized = true;
}

void logMessage(const std::string& message) {
    std::string entry = getTimestamp() + " " + message;
    
    // Store in ring buffer
    g_logEntries[g_logIndex] = entry;
    g_logIndex = (g_logIndex + 1) % MAX_LOG_ENTRIES;
    
#if defined(__ANDROID__)
    LOGD("%s", message.c_str());
    
    // Also append to log file
    if (!g_logPath.empty()) {
        FILE* f = fopen(g_logPath.c_str(), "a");
        if (f) {
            fprintf(f, "%s\n", entry.c_str());
            fclose(f);
        }
    }
#endif
}

std::string getLogPath() {
    return g_logPath;
}

} // namespace crash
