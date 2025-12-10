/*
 * docgen - Blitz3D NG Documentation Generator
 * Generates HTML documentation from Markdown files
 * No external dependencies (Ruby, Python, etc.)
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;

// Simple Markdown to HTML converter
class MarkdownParser {
public:
    std::string parse(const std::string& markdown) {
        std::string html;
        std::istringstream stream(markdown);
        std::string line;
        bool inCodeBlock = false;
        bool inList = false;

        while (std::getline(stream, line)) {
            // Code blocks
            if (line.find("```") == 0) {
                if (inCodeBlock) {
                    html += "</pre></code>\n";
                    inCodeBlock = false;
                } else {
                    html += "<code><pre>";
                    inCodeBlock = true;
                }
                continue;
            }

            if (inCodeBlock) {
                html += escapeHtml(line) + "\n";
                continue;
            }

            // Empty line
            if (line.empty()) {
                if (inList) {
                    html += "</ul>\n";
                    inList = false;
                }
                continue;
            }

            // Headers
            if (line[0] == '#') {
                if (inList) { html += "</ul>\n"; inList = false; }
                int level = 0;
                while (level < line.size() && line[level] == '#') level++;
                std::string text = trim(line.substr(level));
                html += "<h" + std::to_string(level) + ">" + escapeHtml(text) + "</h" + std::to_string(level) + ">\n";
                continue;
            }

            // List items
            if (line.size() > 2 && line[0] == '-' && line[1] == ' ') {
                if (!inList) {
                    html += "<ul>\n";
                    inList = true;
                }
                html += "<li>" + parseInline(line.substr(2)) + "</li>\n";
                continue;
            }

            // Horizontal rule
            if (line == "---" || line == "***") {
                html += "<hr>\n";
                continue;
            }

            // Regular paragraph
            if (inList) { html += "</ul>\n"; inList = false; }
            html += "<p>" + parseInline(line) + "</p>\n";
        }

        if (inList) html += "</ul>\n";
        if (inCodeBlock) html += "</pre></code>\n";

        return html;
    }

private:
    std::string parseInline(const std::string& text) {
        std::string result = escapeHtml(text);

        // Bold **text**
        result = std::regex_replace(result, std::regex("\\*\\*([^*]+)\\*\\*"), "<strong>$1</strong>");

        // Italic *text*
        result = std::regex_replace(result, std::regex("\\*([^*]+)\\*"), "<em>$1</em>");

        // Inline code `code`
        result = std::regex_replace(result, std::regex("`([^`]+)`"), "<kbd>$1</kbd>");

        // Links [text](url)
        result = std::regex_replace(result, std::regex("\\[([^\\]]+)\\]\\(([^)]+)\\)"), "<a href=\"$2\">$1</a>");

        return result;
    }

    std::string escapeHtml(const std::string& text) {
        std::string result;
        for (char c : text) {
            switch (c) {
                case '&': result += "&amp;"; break;
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '"': result += "&quot;"; break;
                default: result += c;
            }
        }
        return result;
    }

    std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t");
        return s.substr(start, end - start + 1);
    }
};

// Documentation generator
class DocGenerator {
public:
    DocGenerator(const std::string& srcPath, const std::string& outPath)
        : srcPath(srcPath), outPath(outPath) {}

    void generate() {
        // Create output directories
        fs::create_directories(outPath);
        fs::create_directories(outPath + "/reference");
        fs::create_directories(outPath + "/assets");

        // Copy assets
        copyAssets();

        // Find all modules with docs
        std::vector<Module> modules;
        findModules(srcPath + "/src/modules/bb", modules);

        // Generate command pages
        for (auto& mod : modules) {
            generateModulePages(mod);
        }

        // Generate index
        generateIndex(modules);

        // Generate reference index
        generateReferenceIndex(modules);

        std::cout << "Documentation generated in " << outPath << std::endl;
    }

private:
    struct Command {
        std::string name;
        std::string filename;
        std::string content;
    };

    struct Module {
        std::string name;
        std::string path;
        std::vector<Command> commands;
    };

    std::string srcPath;
    std::string outPath;
    MarkdownParser parser;

    void findModules(const std::string& path, std::vector<Module>& modules) {
        if (!fs::exists(path)) return;

        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                std::string docsPath = entry.path().string() + "/docs";
                if (fs::exists(docsPath)) {
                    Module mod;
                    mod.name = entry.path().filename().string();
                    mod.path = docsPath;

                    // Find all .md files
                    for (const auto& doc : fs::directory_iterator(docsPath)) {
                        if (doc.path().extension() == ".md") {
                            Command cmd;
                            cmd.name = doc.path().stem().string();
                            cmd.filename = doc.path().string();
                            cmd.content = readFile(cmd.filename);
                            mod.commands.push_back(cmd);
                        }
                    }

                    // Sort commands alphabetically
                    std::sort(mod.commands.begin(), mod.commands.end(),
                        [](const Command& a, const Command& b) { return a.name < b.name; });

                    if (!mod.commands.empty()) {
                        modules.push_back(mod);
                    }
                }
            }
        }

        // Sort modules alphabetically
        std::sort(modules.begin(), modules.end(),
            [](const Module& a, const Module& b) { return a.name < b.name; });
    }

    void generateModulePages(const Module& mod) {
        std::string modDir = outPath + "/reference/" + mod.name;
        fs::create_directories(modDir);

        for (const auto& cmd : mod.commands) {
            std::string html = wrapInLayout(
                cmd.name,
                parser.parse(cmd.content),
                "../../"
            );
            writeFile(modDir + "/" + cmd.name + ".html", html);
        }

        // Module index
        std::string content = "<h1>" + formatModuleName(mod.name) + "</h1>\n<ul>\n";
        for (const auto& cmd : mod.commands) {
            content += "<li><a href=\"" + cmd.name + ".html\">" + cmd.name + "</a></li>\n";
        }
        content += "</ul>\n";

        writeFile(modDir + "/index.html", wrapInLayout(mod.name, content, "../../"));
    }

    void generateIndex(const std::vector<Module>& modules) {
        std::string content = R"(
<div class="full-page">
  <h2>Welcome To Blitz3D "NG"!</h2>
  <p>To return to this page at any time, just click the 'home' icon in the toolbar.</p>

  <ul class="main-menu">
    <li><a href="../samples">Samples</a> - a collection of samples to whet your appetite!</li>
    <li><a href="./reference/index.html">Command Reference</a> - for the low down on the Blitz3D command set.</li>
    <li><a href="https://github.com/nickmckay/blitz3d-ng" target="_blank">GitHub</a> - source code and issue tracker</li>
  </ul>

  <p>If you experience any problems running Blitz3D "NG", please file an issue on
  <a href="https://github.com/nickmckay/blitz3d-ng/issues" target="_blank">GitHub</a>.</p>
</div>
)";
        writeFile(outPath + "/index.html", wrapInLayout("Blitz3D NG Help", content, "./"));
    }

    void generateReferenceIndex(const std::vector<Module>& modules) {
        std::string content = "<h1>Command Reference</h1>\n";

        // All commands alphabetically
        std::vector<std::pair<std::string, std::string>> allCommands;
        for (const auto& mod : modules) {
            for (const auto& cmd : mod.commands) {
                allCommands.push_back({cmd.name, mod.name + "/" + cmd.name + ".html"});
            }
        }
        std::sort(allCommands.begin(), allCommands.end());

        content += "<h2>All Commands</h2>\n<ul>\n";
        for (const auto& [name, path] : allCommands) {
            content += "<li><a href=\"" + path + "\">" + name + "</a></li>\n";
        }
        content += "</ul>\n";

        content += "<h2>By Module</h2>\n";
        for (const auto& mod : modules) {
            content += "<h3><a href=\"" + mod.name + "/index.html\">" + formatModuleName(mod.name) + "</a></h3>\n<ul>\n";
            for (const auto& cmd : mod.commands) {
                content += "<li><a href=\"" + mod.name + "/" + cmd.name + ".html\">" + cmd.name + "</a></li>\n";
            }
            content += "</ul>\n";
        }

        writeFile(outPath + "/reference/index.html", wrapInLayout("Command Reference", content, "../"));
    }

    std::string wrapInLayout(const std::string& title, const std::string& content, const std::string& rootPath) {
        return R"(<!DOCTYPE html>
<html>
<head>
  <title>)" + title + R"( - Blitz3D NG</title>
  <link rel="stylesheet" href=")" + rootPath + R"(assets/style.css" type="text/css" />
</head>
<body>
  <nav>
    <h1>Blitz3D "NG" Docs</h1>
    <a href=")" + rootPath + R"(index.html">
      <img src=")" + rootPath + R"(b3dlogo.jpg" />
    </a>
  </nav>
  <div class="content">
)" + content + R"(
  </div>
</body>
</html>
)";
    }

    void copyAssets() {
        // Generate CSS (simplified from SCSS)
        std::string css = R"(
html, body {
  height: 100%;
  overscroll-behavior: none;
}

* {
  box-sizing: border-box;
  line-height: 1.5;
}

body {
  color: #ffffff;
  text-shadow: 0.5px 0.5px #000000;
  font: 0.9em verdana;
  background: #006060;
  margin: 0;
  display: flex;
  flex-direction: column;
}

h1, h2, h3 {
  color: #00ff44;
  font-family: verdana;
  font-weight: bold;
}

h1 { font-size: 1.5em; }
h2 { font-size: 1em; }

hr {
  border: solid 1px #005858;
  margin: 1em 0;
}

a {
  color: #ffff00;
  text-decoration: none;
}
a:hover {
  color: #00ff44;
  text-decoration: underline;
}

ul {
  color: #ffffff;
  list-style-type: disc;
  padding-left: 10px;
}

ul.main-menu li {
  padding-bottom: 20px;
}

nav {
  padding: 0 32px;
  height: 45px;
  display: flex;
  flex-direction: row;
  border-bottom: 4px solid #005858;
}

nav h1 {
  font-size: 12pt;
  flex: 1;
}

nav img {
  height: 100%;
}

.content {
  padding: 16px 32px;
  overflow: auto;
  flex: 1;
}

.full-page {
  padding: 0 32px;
}

kbd {
  background-color: #225588;
  color: #eeeeee;
  text-shadow: none;
  border: 1px solid #000;
  border-radius: 3px;
  font-family: monospace;
  padding: 2px;
}

code {
  background-color: #225588;
  color: #eeeeee;
  text-shadow: none;
  border: 1px solid #000;
  border-radius: 3px;
  font-family: monospace;
  display: block;
  padding: 0.5em;
  font-size: 1rem;
}

code pre {
  padding: 0;
  margin: 0;
}

p {
  margin: 0.5em 0;
}
)";
        writeFile(outPath + "/assets/style.css", css);

        // Copy logo if exists
        std::string logoSrc = srcPath + "/src/help/views/b3dlogo.jpg";
        std::string logoDst = outPath + "/b3dlogo.jpg";
        if (fs::exists(logoSrc)) {
            fs::copy_file(logoSrc, logoDst, fs::copy_options::overwrite_existing);
        }
    }

    std::string formatModuleName(const std::string& name) {
        // Convert "math" to "Math", "blitz3d" to "Blitz3D", etc.
        std::string result = name;
        if (!result.empty()) {
            result[0] = toupper(result[0]);
        }
        return result;
    }

    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file) return "";
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void writeFile(const std::string& path, const std::string& content) {
        std::ofstream file(path);
        if (file) {
            file << content;
            std::cout << "  Generated: " << path << std::endl;
        } else {
            std::cerr << "  Error writing: " << path << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    std::string srcPath = ".";
    std::string outPath = "_release/help";

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-s" && i + 1 < argc) {
            srcPath = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            outPath = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: docgen [-s srcPath] [-o outPath]\n";
            std::cout << "  -s  Source path (default: .)\n";
            std::cout << "  -o  Output path (default: _release/help)\n";
            return 0;
        }
    }

    std::cout << "Blitz3D NG Documentation Generator\n";
    std::cout << "Source: " << srcPath << "\n";
    std::cout << "Output: " << outPath << "\n\n";

    DocGenerator gen(srcPath, outPath);
    gen.generate();

    return 0;
}
