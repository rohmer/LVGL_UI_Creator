#include "CCodeGenerator.h"
#include <iostream>
int main(int argc, char** argv)
{
    if (argc != 3) { std::cerr << "Usage: UICreatorCodeGen project.json|project.pb output_directory\n"; return 2; }
    CCodeGenerator generator;
    if (!generator.LoadProject(std::string(argv[1])) || !generator.ExportProject(argv[2]))
    { std::cerr << generator.GetLastError() << '\n'; return 1; }
    std::cout << "Generated C project in " << argv[2] << '\n'; return 0;
}
