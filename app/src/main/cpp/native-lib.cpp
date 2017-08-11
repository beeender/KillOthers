#include <jni.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android/log.h>

static const char* TAG = "KILL_OTHERS";

#define LOG_E(msg) \
    __android_log_write(ANDROID_LOG_ERROR, TAG, msg)

#define LOG_W(msg) \
    __android_log_write(ANDROID_LOG_WARN, TAG, msg)

// Checks if the /proc/pid belongs the given uid.
static bool check_proc_folder_uid(uid_t uid, pid_t pid) {
    char path[256];
    snprintf(path, 256, "/proc/%d", pid);
    struct stat dir_stat;
    if (stat(path, &dir_stat) != 0) {
        return false;
    }
    return dir_stat.st_uid == uid;
}

// Gets the process name by the given pid.
static std::string get_process_name(pid_t pid) {
    const static size_t max_cmdline_length = 4096;
    char path[256];
    std::unique_ptr<char> cmdline(new char[max_cmdline_length]);
    snprintf(path, 256, "/proc/%d/cmdline", pid);
    FILE* file = fopen(path, "r");
    char* results = fgets(cmdline.get(), max_cmdline_length, file);
    fclose(file);
    if (results) {
        return std::string(cmdline.get());
    }
    return {};
}

// Sends SIGKILL to the given pid.
static void kill_by_pid(pid_t pid, size_t tried_times) {
    static const std::vector<useconds_t> sleep_time = {50*1000, 100*1000, 200*1000};
    if (tried_times >= sleep_time.size()) {
        LOG_E("Killing failed.");
        return;
    }
    if (kill(pid, SIGKILL) == 0) {
        usleep(sleep_time[tried_times]);
        kill_by_pid(pid, tried_times + 1);
    } else {
        char log_buf[100];
        int error = errno;
        if (error == ESRCH) {
            snprintf(log_buf, 100, "The old process %d has been killed.", pid);
            LOG_W(log_buf);
        } else {
            snprintf(log_buf, 100, "Killing failed with errno: %d.", error);
            LOG_E(log_buf);
        }
    }
}

// This function try to check if any other process is alive which has the same name and the same uid as the current
// one's by iterating /proc/. Then try to kill the other with SIGKILL.
static void kill_others() {
    static const size_t log_buf_size = 100;
    char log_buf[log_buf_size];

    DIR* proc = opendir("/proc");
    if (!proc) {
        int error = errno;
        snprintf(log_buf, log_buf_size, "'/proc' cannot be opened. errno: %d.", error);
        LOG_E(log_buf);
        return;
    }

    uid_t my_uid = getuid();
    pid_t my_pid = getpid();
    std::string my_process_name;
    std::map<pid_t, std::string> pid_to_name_map;

    struct dirent* ent;
    while(ent = readdir(proc)) {
        if (!isdigit(*ent->d_name)) continue;

        pid_t pid = static_cast<uid_t>(strtol(ent->d_name, NULL, 10));

        if (!check_proc_folder_uid(my_uid, pid)) continue;

        std::string process_name = get_process_name(pid);
        if (pid == my_pid) {
            my_process_name = std::move(process_name);
        } else {
            pid_to_name_map[pid] = std::move(process_name);
        }
    }

    closedir(proc);

    for (auto& pid_and_name : pid_to_name_map) {
        if (my_process_name.compare(pid_and_name.second) == 0) {
            kill_by_pid(pid_and_name.first, 0);
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_io_beeender_killothers_Killer_killOthers(JNIEnv*, jclass) {
    kill_others();
}

extern "C"
JNIEXPORT jstring JNICALL
Java_io_beeender_killothers_Killer_getMyProcessName(JNIEnv* env, jclass) {
    pid_t my_pid = getpid();

    return env->NewStringUTF(get_process_name(my_pid).c_str());
}