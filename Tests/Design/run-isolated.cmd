@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b %errorlevel%
if not exist build\design-check mkdir build\design-check
cl /nologo /std:c++17 /EHsc /utf-8 /DPROJECT_DESIGN_STANDALONE Tests\Design\ProjectDesign.cpp /Fo:build\design-check\ProjectDesign.obj /Fe:build\design-check\ProjectDesign.exe
if errorlevel 1 exit /b %errorlevel%
build\design-check\ProjectDesign.exe
