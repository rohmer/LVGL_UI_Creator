# Corpus language workflow checks

Build/run this Windows .NET 8 console test project from either ESPOS or LVGL UI Creator. Pass the LVGL UI Creator source root, which supplies the original C# model reference fixtures, and optionally the optimized native helper executable:

```powershell
dotnet run --project tools/CorpusLanguageTests/CorpusLanguageTests.csproj -c Release -- `
  E:/source/LVGL_UI_Creator E:/source/LVGL_UI_Creator/Tools/NativeTrieGenerator/bin/UICreatorModelGen.exe
```

The suite checks eleven unchanged legacy C# NGTR models; independently expected counts/weights for Arabic, Hindi, Mandarin, Bengali, Indonesian, Urdu and Russian; Unicode marks/joiners and Han character boundaries; managed memory/SQLite output parity; vocabulary rejection and language-specific caches; generator UI handoffs; real compressed Wikipedia exports; and a crawler using a temporary local HTTP server. With a native helper, it also compares all seven C#/C++ models and stops a real spilling generation while preserving previous outputs and cleaning scratch storage.

All generated files stay below the supplied checkout's ignored `build/corpus-language-tests` directory. No external website, existing corpus, or saved user setting is changed. Windows UI controls are constructed without showing windows. No Java or OpenNLP model download is needed for these seven fallback profiles.
