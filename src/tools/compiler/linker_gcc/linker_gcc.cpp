#include "linker_gcc.h"
#include "../std.h"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>

#ifdef BB_POSIX
#include <unistd.h>
#include <sys/stat.h>
#endif

Linker_GCC::Linker_GCC(const std::string &home) : home(home), env(nullptr) {
}

std::string Linker_GCC::getCompiler(const Target &target) {
    // Check for Android target
    if (target.type == "android" || target.type == "ovr") {
        char *ndkhome = getenv("ANDROID_NDK_HOME");
        if (ndkhome) {
            std::string ndk = std::string(ndkhome);
            std::string arch = "aarch64";
            if (target.arch == "armeabi-v7a") arch = "armv7a";
            else if (target.arch == "x86_64") arch = "x86_64";
            else if (target.arch == "x86") arch = "i686";

            // Extract API level
            std::string api = target.version;
            size_t dashpos = api.rfind('-');
            if (dashpos != std::string::npos) {
                api = api.substr(dashpos + 1);
            }

            std::string triple = arch + "-linux-android" + api;
#ifdef __APPLE__
            std::string host = "darwin-x86_64";
#else
            std::string host = "linux-x86_64";
#endif
            return ndk + "/toolchains/llvm/prebuilt/" + host + "/bin/" + triple + "-clang";
        }
    }

    // Native compilation
#ifdef __APPLE__
    return "clang";
#else
    // Prefer clang if available, fall back to gcc
    if (system("which clang > /dev/null 2>&1") == 0) {
        return "clang";
    }
    return "gcc";
#endif
}

std::string Linker_GCC::getLinker(const Target &target) {
    // For most cases, use the same as compiler (gcc/clang handles linking)
    return getCompiler(target);
}

std::vector<std::string> Linker_GCC::getLibPaths(const Target &target) {
    std::vector<std::string> paths;

    std::string toolchain = home + "/bin/" + target.triple;
    paths.push_back(toolchain + "/lib");

    if (target.type == "android" || target.type == "ovr") {
        char *ndkhome = getenv("ANDROID_NDK_HOME");
        if (ndkhome) {
            std::string ndk = std::string(ndkhome);
            std::string arch = "aarch64-linux-android";
            if (target.arch == "armeabi-v7a") arch = "arm-linux-androideabi";
            else if (target.arch == "x86_64") arch = "x86_64-linux-android";
            else if (target.arch == "x86") arch = "i686-linux-android";

            std::string api = target.version;
            size_t dashpos = api.rfind('-');
            if (dashpos != std::string::npos) {
                api = api.substr(dashpos + 1);
            }

#ifdef __APPLE__
            std::string host = "darwin-x86_64";
#else
            std::string host = "linux-x86_64";
#endif
            std::string sysroot = ndk + "/toolchains/llvm/prebuilt/" + host + "/sysroot";
            paths.push_back(sysroot + "/usr/lib/" + arch + "/" + api);
            paths.push_back(sysroot + "/usr/lib/" + arch);
        }
    }

    return paths;
}

std::vector<std::string> Linker_GCC::getLibraries(const Target &target) {
    std::vector<std::string> libs;

    // Base runtime libraries
    libs.push_back("m");      // math
    libs.push_back("pthread");

    if (target.type == "android" || target.type == "ovr") {
        libs.push_back("log");
        libs.push_back("android");
        libs.push_back("EGL");
        libs.push_back("GLESv3");
    } else {
#ifdef __linux__
        libs.push_back("dl");
        libs.push_back("GL");
#endif
    }

    return libs;
}

int Linker_GCC::compileToObject(const std::string &cFile, const std::string &objFile,
                                 const Target &target, bool debug) {
    std::stringstream cmd;
    cmd << getCompiler(target);

    // Compilation flags
    cmd << " -c";
    if (debug) {
        cmd << " -g -O0";
    } else {
        cmd << " -O2";
    }

    // Output
    cmd << " -o " << shellQuote(objFile);

    // Input
    cmd << " " << shellQuote(cFile);

    std::cout << "Compiling: " << cmd.str() << std::endl;
    return system(cmd.str().c_str());
}

int Linker_GCC::linkExecutable(const std::string &objFile, const std::string &exeFile,
                                const Target &target, bool debug,
                                const std::vector<std::string> &extraLibs) {
    std::stringstream cmd;
    cmd << getLinker(target);

    // ld64 (Apple's native linker) has no --start/--end-group (it resolves
    // archive cycles by itself) and treats -s as obsolete.
#ifdef __APPLE__
    bool ld64 = !(target.type == "android" || target.type == "ovr");
#else
    bool ld64 = false;
#endif

    // Link flags
    if (debug) {
        cmd << " -g";
    } else {
        if (!ld64) cmd << " -s"; // strip symbols
    }

    // Output
    cmd << " -o " << shellQuote(exeFile);

    // Input object
    cmd << " " << shellQuote(objFile);

    // Library paths
    for (const auto &path : getLibPaths(target)) {
        cmd << " -L" << shellQuote(path);
    }

    // Extra libraries from caller
    for (const auto &lib : extraLibs) {
        cmd << " -l" << lib;
    }

    // System libraries
    for (const auto &lib : getLibraries(target)) {
        cmd << " -l" << lib;
    }

    std::cout << "Linking: " << cmd.str() << std::endl;
    return system(cmd.str().c_str());
}

std::string Linker_GCC::getGccCommand(bool debug, const std::string &rt, const Target &target,
                                       const std::string &cFile, const std::string &exeFile) {
    // For GCC backend, we compile and link in one step
    std::stringstream cmd;
    cmd << getCompiler(target);

    // ld64 (Apple's native linker) has no --start/--end-group (it resolves
    // archive cycles by itself) and treats -s as obsolete.
#ifdef __APPLE__
    bool ld64 = !(target.type == "android" || target.type == "ovr");
#else
    bool ld64 = false;
#endif

    // Compilation and link flags
    if (debug) {
        cmd << " -g -O0";
    } else {
        cmd << " -O2";
        if (!ld64) cmd << " -s";
    }

    // Output
    cmd << " -o " << shellQuote(exeFile);

    // Input C file
    cmd << " " << shellQuote(cFile);

    // Library paths
    for (const auto &path : getLibPaths(target)) {
        cmd << " -L" << shellQuote(path);
    }

    // Use --start-group to handle circular dependencies between libraries
    if (!ld64) cmd << " -Wl,--start-group";

    // Runtime library
    cmd << " -lruntime." << rt << ".static";

    // Get modules from target
    auto rti = target.runtimes.find(rt);
    if (rti != target.runtimes.end()) {
        for (const std::string &mod : rti->second.modules) {
            cmd << " -lbb." << mod;
            auto mi = target.modules.find(mod);
            if (mi != target.modules.end()) {
                for (const std::string &lib : mi->second.libs) {
                    cmd << " -l" << lib;
                }
            }
        }
    }

    if (!ld64) cmd << " -Wl,--end-group";

    // System libraries (after --end-group)
    if (rti != target.runtimes.end()) {
        for (const std::string &mod : rti->second.modules) {
            auto mi = target.modules.find(mod);
            if (mi != target.modules.end()) {
                for (const std::string &lib : mi->second.system_libs) {
                    // Handle frameworks on macOS
                    if (lib.find("-framework") == 0) {
                        cmd << " " << lib;
                    } else {
                        cmd << " -l" << lib;
                    }
                }
            }
        }
    }

    // Additional system libraries
    for (const auto &lib : getLibraries(target)) {
        cmd << " -l" << lib;
    }

    // C++ standard library
    cmd << " -lstdc++";

    return cmd.str();
}

int Linker_GCC::createExe(bool debug, const std::string &rt, const Target &target,
                           const std::string &cFile, const std::string &exeFile) {
    std::string cmd = getGccCommand(debug, rt, target, cFile, exeFile);

    int ret = system(cmd.c_str());

    if (ret != 0) {
        std::cerr << "Failed to build executable" << std::endl;
    }

    return ret;
}
