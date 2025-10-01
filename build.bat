@echo off

set target_output=%1

:: Get current epoch unix timestamp
for /f %%i in ('powershell -NoProfile -Command "[int][DateTimeOffset]::UtcNow.ToUnixTimeSeconds()"') do set unix_epoch=%%i

if not exist build (
	echo Creating build directory
	mkdir build
)

pushd build

del *.pdb > NUL 2> NUL

:: Decide what to build
if "%target_output%"=="" (
    call :build_executable
    call :build_dll
) else if "%target_output%"=="executable" (
    call :build_executable
) else if "%target_output%"=="dll" (
    call :build_dll
) else (
    echo Unknown target: %target_output%
)

goto end

:: Windows executable
:build_executable
set executable_name=chessWinX64Debug
set sources=..\src\win32_chess.cpp
set include_dirs=..\external
set compiler_opts=/nologo /FC /Zi /I%include_dirs% /Fe%executable_name% -D"CHESS_BUILD_DEBUG=1" -D"CHESS_PLATFORM_WINDOWS=1"
set linker_opts=user32.lib opengl32.lib gdi32.lib
echo Building exectuable
cl %compiler_opts% %sources% /link %linker_opts% -incremental:no /PDB:%executable_name%_%unix_epoch%.pdb
goto :eof

:: Game DLL
:build_dll
set sources=..\src\chess.cpp
set include_dirs=..\external
set compiler_opts=/nologo /Zi /I%include_dirs% /FC /LD /FmChess
set linker_opts=-incremental:no /PDB:Chess_%unix_epoch%.pdb -EXPORT:GameUpdateAndRender
echo Building DLL
cl %compiler_opts% %sources% /link %linker_opts%
goto :eof

:end
popd