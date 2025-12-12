#ifndef LINKER_GCC_H
#define LINKER_GCC_H

#include "../environ.h"
#include "../target.h"
#include <string>
#include <vector>

class Linker_GCC {
public:
    std::string home;
    const Environ *env;

    Linker_GCC(const std::string &home);

    // Compile C source to object file
    int compileToObject(const std::string &cFile, const std::string &objFile,
                        const Target &target, bool debug);

    // Link object files into executable
    int linkExecutable(const std::string &objFile, const std::string &exeFile,
                       const Target &target, bool debug,
                       const std::vector<std::string> &libs);

    // Combined: compile and link
    int createExe(bool debug, const std::string &rt, const Target &target,
                  const std::string &cFile, const std::string &exeFile);

    // Get the gcc command string without executing
    std::string getGccCommand(bool debug, const std::string &rt, const Target &target,
                              const std::string &cFile, const std::string &exeFile);

private:
    // Get compiler command for target
    std::string getCompiler(const Target &target);

    // Get linker command for target
    std::string getLinker(const Target &target);

    // Get library paths for target
    std::vector<std::string> getLibPaths(const Target &target);

    // Get required libraries
    std::vector<std::string> getLibraries(const Target &target);
};

#endif
