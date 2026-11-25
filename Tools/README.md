# Advanced Keyboard tools

Keyboard tools are maintained only in the pinned
[`3rdParty/AdvancedKeyboard/tools`](../3rdParty/AdvancedKeyboard/tools) repository.
Initialize it with `git submodule update --init --recursive`.

```sh
dotnet build 3rdParty/AdvancedKeyboard/tools/CorpusWorkbench/CorpusWorkbench.csproj -c Release
cmake -S 3rdParty/AdvancedKeyboard/tools -B build/keyboard-tools -DCMAKE_BUILD_TYPE=Release
cmake --build build/keyboard-tools --config Release --parallel
```

See the dependency's [tool guide](../3rdParty/AdvancedKeyboard/tools/README.md)
for helper discovery, simulator and generation options. Its build outputs stay
ignored; source changes belong in the AdvancedKeyboard repository. Old native
CMake entry points here are compatibility bridges with no trainer/codec code.
Existing downloaded resources, settings and build outputs are preserved. Run
`python Integrations/AdvancedKeyboard/dependency.py --init` to reuse existing
ignored OpenNLP downloads in the dependency's tool workspace.
