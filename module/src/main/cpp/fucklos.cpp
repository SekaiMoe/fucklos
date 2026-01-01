#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <map>

#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "fucklos"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

class fucklos : public zygisk::ModuleBase {
public:
    void onLoad(Api *api_, JNIEnv *env_) override {
        this->api = api_;
        this->env = env_;
        packages_loaded = false;
        LOGI("Module loaded");
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);

        std::string process(args->nice_name);
        LOGD("Processing: %s", process.c_str());

        // Skip non-main processes (contain ':')
        if (process.find(':') != std::string::npos) {
            LOGD("Skipped non-main process");
            return;
        }

        if (!packages_loaded) {
            loadPackageList();
            packages_loaded = true;
        }

        if (std::find(target_packages.begin(), target_packages.end(), process) == target_packages.end()) {
            LOGD("Not in target list");
            return;
        }

        LOGI("Hooking target: %s", process.c_str());
        hideSensitiveBuildInfo();
    }

    void postServerSpecialize([[maybe_unused]] const ServerSpecializeArgs *args) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<std::string> target_packages;
    bool packages_loaded = false;

    // Helper: trim string (in-place)
    void trim(std::string &s) {
        // Trim front
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) {
            return !std::isspace(c);
        }));
        // Trim back
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) {
            return !std::isspace(c);
        }).base(), s.end());
    }

    void loadPackageList() {
        char path[PATH_MAX];
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

        char line[512];
        int count = 0;
        while (fgets(line, sizeof(line), file)) {
            std::string s(line);
            trim(s);
            if (s.empty() || s[0] == '#') continue;
            target_packages.push_back(s);
            LOGD("Added: %s", s.c_str());
            count++;
        }
        fclose(file);
        LOGI("Loaded %d packages", count);
    }

    bool containsIgnoreCase(const std::string& str, const std::string& pattern) {
        std::string lower_str = str;
        std::string lower_pat = pattern;
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
        std::transform(lower_pat.begin(), lower_pat.end(), lower_pat.begin(), ::tolower);
        return lower_str.find(lower_pat) != std::string::npos;
    }

    std::string replaceAllIgnoreCase(std::string str, const std::string& from, const std::string& to) {
        std::string result = str;
        std::string lower = result;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        std::string lower_from = from;
        std::transform(lower_from.begin(), lower_from.end(), lower_from.begin(), ::tolower);

        size_t pos = 0;
        while ((pos = lower.find(lower_from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            lower.replace(pos, from.length(), to);
            pos += to.length();
        }
        return result;
    }

    void hideSensitiveBuildInfo() {
        jclass build_class = env->FindClass("android/os/Build");
        if (!build_class) {
            LOGE("Failed to find Build class");
            return;
        }

        const char* fields[] = {
            "FINGERPRINT", "DISPLAY", "MANUFACTURER", "MODEL",
            "BRAND", "DEVICE", "PRODUCT", "ID", "TAGS", "TYPE", nullptr
        };

        std::vector<std::pair<std::string, std::string>> replacements = {
            {"LineageOS", "StockOS"},
            {"Lineage",   "Stock"},
            {"los",       "stock"},
            {"userdebug", "user"}
        };

        for (int i = 0; fields[i]; ++i) {
            jfieldID fid = env->GetStaticFieldID(build_class, fields[i], "Ljava/lang/String;");
            if (!fid) continue;

            jstring original = (jstring)env->GetStaticObjectField(build_class, fid);
            if (!original) continue;

            const char* cstr = env->GetStringUTFChars(original, nullptr);
            if (!cstr) continue;

            std::string value(cstr);
            env->ReleaseStringUTFChars(original, cstr);

            std::string new_value = value;
            bool changed = false;
            for (const auto& [pattern, replacement] : replacements) {
                if (containsIgnoreCase(new_value, pattern)) {
                    new_value = replaceAllIgnoreCase(new_value, pattern, replacement);
                    changed = true;
                }
            }

            if (changed) {
                LOGI("Modified %s: '%s' → '%s'", fields[i], value.c_str(), new_value.c_str());
                jstring jnew = env->NewStringUTF(new_value.c_str());
                env->SetStaticObjectField(build_class, fid, jnew);
                env->DeleteLocalRef(jnew);
            }
        }
    }
};

REGISTER_ZYGISK_MODULE(fucklos)
