@echo off

if not exist build\release (
	echo Creating build directory
	mkdir build\release
)

pushd build\release

set preprocessor=-D"CHESS_BUILD_RELEASE=1" -D"NOMINMAX=1" -D"WIN32_LEAN_AND_MEAN=1"
set include_dirs=..\..\external

:: Windows executable
set executable_name=chessWinX64Release
set sources=..\..\src\win32_chess.cpp
set compiler_opts=/nologo /FC /I%include_dirs% /Fe%executable_name% %preprocessor% /std:c++17 /EHsc /GS /GL /O2 /Oi
set linker_opts=user32.lib opengl32.lib gdi32.lib ..\..\external\freetype.lib /OPT:ICF
echo Building exectuable
cl %compiler_opts% %sources% /link %linker_opts% -incremental:no /PDB:%executable_name%_%unix_epoch%.pdb

:: Game DLL
set sources=..\..\src\chess.cpp
set linker_opts=/OPT:ICF /DLL /EXPORT:GameUpdateAndRender /OUT:chess.dll
set compiler_opts=/nologo /I%include_dirs% /FC /LD %preprocessor% /std:c++17 /EHsc /GS /GL /O2 /Oi

echo Building DLL
cl %compiler_opts% %sources% /link %linker_opts%

popd