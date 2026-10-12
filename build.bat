@echo off
rem ============================================================
rem  build.bat - skrypt budowy gitlense dla Windows (MSVC)
rem ============================================================
setlocal enabledelayedexpansion

set "SOURCE_DIR=%~dp0"
set "BUILD_DIR=%SOURCE_DIR%build"
set "CONFIG=Release"
set "CLEAN=0"
emset "RUN_TESTS=0"
set "FETCH_LIBGIT2=1"

rem --- Parsowanie argumentow -----------------------------------
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--debug" set "CONFIG=Debug"
if /i "%~1"=="--release" set "CONFIG=Release"
if /i "%~1"=="--clean" set "CLEAN=1"
if /i "%~1"=="--test" set "RUN_TESTS=1"
if /i "%~1"=="--no-fetch-libgit2" set "FETCH_LIBGIT2=0"
shift
goto parse_args
:args_done

rem --- Sprawdz czy cmake jest dostepny --------------------------
where cmake >nul 2>&1
if errorlevel 1 (
    echo [BLAD] cmake nie zostal znaleziony w PATH.
    echo        Zainstaluj CMake 3.20+ i dodaj go do PATH.
    exit /b 1
)

rem --- Opcjonalny czysty build ---------------------------------
if "%CLEAN%"=="1" (
    echo [INFO] Czyszczenie katalogu build ...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem --- Konfiguracja ---------------------------------------------
echo [INFO] Konfiguracja CMake ^(%CONFIG%^) ...
cmake -S "%SOURCE_DIR%" -B "%BUILD_DIR%" ^
  -DCMAKE_BUILD_TYPE=%CONFIG% ^
  -DGITLENSE_FETCH_LIBGIT2=%FETCH_LIBGIT2%

if errorlevel 1 (
    echo [BLAD] Konfiguracja CMake nie powiodla sie.
    exit /b 1
)

rem --- Budowa ---------------------------------------------------
echo [INFO] Kompilacja ...
cmake --build "%BUILD_DIR%" --config %CONFIG% -j

if errorlevel 1 (
    echo [BLAD] Kompilacja nie powiodla sie.
    exit /b 1
)

echo [OK] Zakonczono pomyslnie.
echo      Binary: %BUILD_DIR%\%CONFIG%\gitlense.exe

rem --- Opcjonalne testy ----------------------------------------
if "%RUN_TESTS%"=="1" (
    echo [INFO] Uruchamianie testow ...
    ctest --test-dir "%BUILD_DIR%" -C %CONFIG% --output-on-failure
)

endlocal
