#ifndef CODEGEN_C_H
#define CODEGEN_C_H

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include "../target.h"

// Forward declarations for Blitz types
struct Type;
struct Environ;

class Codegen_C {
public:
    Codegen_C(bool debug);
    ~Codegen_C();

    std::string target = "native";
    bool debug;
    bool gosubUsed = false;

    // Output streams
    std::stringstream header;      // Forward declarations, structs
    std::stringstream globals;     // Global variables
    std::stringstream functions;   // Function definitions
    std::stringstream mainCode;    // Main program code
    std::stringstream dataSection; // DATA statements

    // Current output target
    std::stringstream *current;

    // Indentation level
    int indentLevel;

    // Label and variable tracking
    std::map<std::string, bool> labels;
    std::map<std::string, int> arrays;
    std::map<std::string, std::string> strings;
    int stringCounter;
    int tempCounter;
    int labelCounter;

    // Local variable tracking
    std::map<std::string, std::string> localVars; // var name -> C type

    // Break label stack for Exit statements
    std::vector<std::string> breakLabelStack;

    // Data values
    std::vector<std::string> dataValues;

    // Methods
    void SetTarget(const Target &target);

    // Output helpers
    void indent();
    void outdent();
    void emit(const std::string &code);
    void emitLine(const std::string &code);
    void emitLabel(const std::string &label);
    void emitGlobal(const std::string &code);

    // Type conversion
    std::string toCType(Type *type);
    std::string toCSafeName(const std::string &name);

    // Constants
    std::string constantInt(int i);
    std::string constantFloat(double f);
    std::string constantString(const std::string &s);

    // Temporary variables
    std::string newTemp();

    // Labels
    std::string getLabel(const std::string &ident);

    // Local variables
    void declareLocal(const std::string &name, const std::string &cType);

    // Array handling
    void declareArray(const std::string &ident, int dims);

    // Code generation entry points
    void beginMain();
    void endMain();
    void beginFunction(const std::string &name, const std::string &returnType, const std::vector<std::pair<std::string, std::string>> &params);
    void endFunction();

    // Runtime calls
    std::string callRuntime(const std::string &func, const std::vector<std::string> &args);

    // Final output
    std::string generateOutput();

    // Compile the generated C code
    int compileToObject(const std::string &outputPath);
};

#endif
