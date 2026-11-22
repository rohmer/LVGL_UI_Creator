#include "CCodeGenerator.h"
#include "MicroPythonCodeGenerator.h"
#include "XmlCodeGenerator.h"
#include <iostream>
#include <memory>
#include <filesystem>
#include <vector>
int main(int argc, char** argv)
{
    std::vector<std::string> positional;
    std::string format = "c";
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--format" && index + 1 < argc) format = argv[++index];
        else if (argument.rfind("--format=", 0) == 0) format = argument.substr(9);
        else if (argument == "--micropython") format = "micropython";
        else positional.push_back(argument);
    }
    if (positional.size() != 2 || (format != "c" && format != "micropython" && format != "xml")) {
        std::cerr << "Usage: UICreatorCodeGen [--format c|micropython|xml] project.json|project.pb|project.xml output_path\n";
        return 2;
    }
    std::unique_ptr<CodeGeneratorBase> generator;
    std::error_code pathError;
    if (std::filesystem::equivalent(std::filesystem::u8path(positional[0]), std::filesystem::u8path(positional[1]), pathError)) {
        std::cerr << "Output cannot overwrite the input project\n"; return 1;
    }
    if (format == "micropython") generator = std::make_unique<MicroPythonCodeGenerator>();
    else if (format == "xml") generator = std::make_unique<XmlCodeGenerator>();
    else generator = std::make_unique<CCodeGenerator>();
    if (!generator->LoadProject(positional[0]) || !generator->ExportProject(positional[1]))
    { std::cerr << generator->GetLastError() << '\n'; return 1; }
    std::cout << "Generated " << (format == "xml" ? "XML" : format == "micropython" ? "MicroPython" : "C") << " project in " << positional[1] << '\n'; return 0;
}
