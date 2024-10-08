@echo off

set MODE=%1
if [%1]==[] (
  set MODE=debug
)

if not exist bin mkdir bin
set WARNING_FLAGS=/W4 /wd4100 /wd4101 /wd4189 /wd4201 /wd4456 /wd4505 /wd4706
set INCLUDES=/Isrc\ /Iext\ /Iext\freetype\include\
set COMMON_COMPILER_FLAGS=/nologo /FC /EHsc /Fdbin\ /Fobin\ %WARNING_FLAGS% %INCLUDES%
set COMMON_LINKER_FLAGS=/INCREMENTAL:NO /OPT:REF

set LIBS=/LIBPATH:ext\freetype\libs\x64\Release\ user32.lib shell32.lib kernel32.lib winmm.lib shlwapi.lib freetype.lib
set COMPILER_FLAGS=%COMMON_COMPILER_FLAGS%
set LINKER_FLAGS=%COMMON_LINKER_FLAGS% %LIBS%
if %MODE%==release (
  set COMPILER_FLAGS=/O2 /D %COMPILER_FLAGS%
  set LINKER_FLAGS=/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup %LINKER_FLAGS%
) else if %mode%==debug (
  set COMPILER_FLAGS=/Zi /Od %COMPILER_FLAGS%
) else (
  echo Unkown build mode
  exit /B 2
)

::CL %COMPILER_FLAGS% src\meta\meta_generator.cpp /Femeta_generator.exe /link %COMMON_LINKER_FLAGS%
CL %COMPILER_FLAGS% src\pix3\pix3_main.cpp /FePix3.exe /link %LINKER_FLAGS%
