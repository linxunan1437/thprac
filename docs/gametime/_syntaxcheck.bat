@echo off
setlocal enabledelayedexpansion
call "D:\tools\VS\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul

set SRC=D:\workshop\thprac\thprac\src
set INC=/I "%SRC%" /I "%SRC%\thprac" /I "%SRC%\3rdParties\rapidjson\include" /I "%SRC%\3rdParties\MetroHash" /I "%SRC%\3rdParties\distorm\include" /I "%SRC%\3rdParties\ImGui" /I "%SRC%\3rdParties\FreeType\include" /I "%SRC%\3rdParties\MinHook\include"

set DEFS=/DUNICODE /D_UNICODE /DWIN32 /DHAVE_SSE2 /D_CRT_SECURE_NO_WARNINGS /D_WINDOWS /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DDISTORM_LIGHT /DNDEBUG

for %%F in (thprac_launcher_others.cpp thprac_launcher_games.cpp thprac_load_exe.cpp thprac_main.cpp thprac_launcher_main.cpp) do (
    echo ---- %%F ----
    cl /nologo /Zs /std:c++20 /utf-8 /EHsc /W3 %DEFS% %INC% "%SRC%\thprac\%%F"
    echo EXIT=!ERRORLEVEL!
)
endlocal
