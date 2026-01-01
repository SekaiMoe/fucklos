#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "fucklos"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

#define MAX_PACKAGES 64
#define MAX_PKG_LEN 128

class fucklos : public zygisk::ModuleBase {
public:
    void onLoad(Api *api_, JNIEnv *env_) override {
        this->api = api_;
        this->env = env_;
        package_count = 0;
        LOGI("Module loaded");
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!process) return;

        LOGD("Processing: %s", process);

        bool should_hook = true;

        // Skip non-main processes
        if (strchr(process, ':') != nullptr) {
            should_hook = false;
        } else {
            if (package_count == 0) {
                loadPackageList();
            }
            should_hook = false;
            for (int i = 0; i < package_count; ++i) {
                if (strcmp(target_packages[i], process) == 0) {
                    should_hook = true;
                    break;
                }
            }
        }

        if (should_hook) {
            LOGI("Hooking target: %s", process);
            hideSensitiveBuildInfo();
        } else {
            LOGD("Skipped: not a target main process");
        }

        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postServerSpecialize([[maybe_unused]] const ServerSpecializeArgs *args) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    char target_packages[MAX_PACKAGES][MAX_PKG_LEN];
    int package_count = 0;

    // Helper: trim whitespace from both ends (modifies in-place)
    void trim(char *s) {
        // Trim front
        char *p = s;
        while (*p && isspace((unsigned char)*p)) p++;
        if (p != s) memmove(s, p, strlen(p) + 1);

        // Trim back
        size_t len = strlen(s);
        while (len > 0 && isspace((unsigned char)s[len - 1])) {
            s[--len] = '\0';
        }
    }

    void loadPackageList() {
        char path[256];
        if (const char* mod = getenv("ZYGISK_MOD_PATH")) {
            snprintf(path, sizeof(path), "%s/package.list", mod);
        } else {
            snprintf(path, sizeof(path), "/data/adb/modules/fucklos/package.list");
        }

        LOGD("Loading package list from: %s", path);

        FILE* file = fopen(path, "r");
        if (!file) {
            LOGE("Failed to open package.list");
            return;
        }

        char line[256];
        int count = 0;
        while (fgets(line, sizeof(line), file) && count < MAX_PACKAGES) {
            trim(line);
            if (line[0] == '\0' || line[0] == '#') continue;
            strncpy(target_packages[count], line, MAX_PKG_LEN - 1);
            target_packages[count][MAX_PKG_LEN - 1] = '\0';
            LOGD("Added: %s", target_packages[count]);
            count++;
        }
        fclose(file);
        package_count = count;
        LOGI("Loaded %d packages", count);
    }

    // Case-insensitive substring search
    bool containsIgnoreCase(const char* str, const char* pattern) {
        if (!str || !pattern) return false;
        size_t len1 = strlen(str);
        size_t len2 = strlen(pattern);
        if (len2 == 0) return true;
        if (len2 > len1) return false;

        for (size_t i = 0; i <= len1 - len2; ++i) {
            bool match = true;
            for (size_t j = 0; j < len2; ++j) {
                if (tolower((unsigned char)str[i + j]) != tolower((unsigned char)pattern[j])) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
        return false;
    }

    // Replace all case-insensitive occurrences (in-place on buffer)
    void replaceAllIgnoreCase(char* buffer, size_t buf_size, const char* from, const char* to) {
        if (!buffer || !from || !to) return;
        size_t from_len = strlen(from);
        size_t to_len = strlen(to);
        if (from_len == 0) return;

        char temp[512];
        size_t temp_len = 0;
        size_t i = 0;
        size_t buf_len = strlen(buffer);

        while (i <= buf_len && temp_len < sizeof(temp) - 1) {
            if (buf_len - i >= from_len &&
                strncasecmp(buffer + i, from, from_len) == 0) {
                // Found match
                if (temp_len + to_len >= sizeof(temp)) break;
                memcpy(temp + temp_len, to, to_len);
                temp_len += to_len;
                i += from_len;
            } else {
                if (temp_len + 1 >= sizeof(temp)) break;
                temp[temp_len++] = buffer[i++];
            }
        }
        temp[temp_len] = '\0';
        strncpy(buffer, temp, buf_size - 1);
        buffer[buf_size - 1] = '\0';
    }

    void hideSensitiveBuildInfo() {
        jclass build_class = env->FindClass("android/os/Build");
        if (!build_class) {
            LOGE("Failed to find Build class");
            return;
        }

        const char* fields[] = {
            "FINGERPRINT", "DISPLAY", "MANUFACTURER", "MODEL",
            "BRAND", "DEVICE", "PRODUCT", "ID", "TAGS", "TYPE", NULL
        };

        const char* patterns[] = {"LineageOS", "Lineage", "los", "userdebug"};
        const char* replacements[] = {"StockOS", "Stock", "stock", "user"};
        int num_replacements = 4;

        for (int i = 0; fields[i] != NULL; ++i) {
            jfieldID fid = env->GetStaticFieldID(build_class, fields[i], "Ljava/lang/String;");
            if (!fid) continue;

            jstring original = (jstring)env->GetStaticObjectField(build_class, fid);
            if (!original) continue;

            const char* cstr = env->GetStringUTFChars(original, NULL);
            if (!cstr) continue;

            char value[512];
            strncpy(value, cstr, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';
            env->ReleaseStringUTFChars(original, cstr);

            char new_value[512];
            strncpy(new_value, value, sizeof(new_value) - 1);
            new_value[sizeof(new_value) - 1] = '\0';

            bool changed = false;
            for (int j = 0; j < num_replacements; ++j) {
                if (containsIgnoreCase(new_value, patterns[j])) {
                    replaceAllIgnoreCase(new_value, sizeof(new_value), patterns[j], replacements[j]);
                    changed = true;
                }
            }

            if (changed) {
                LOGI("Modified %s: '%s' → '%s'", fields[i], value, new_value);
                jstring jnew = env->NewStringUTF(new_value);
                env->SetStaticObjectField(build_class, fid, jnew);
                env->DeleteLocalRef(jnew);
            }
        }
    }
};

REGISTER_ZYGISK_MODULE(fucklos)
