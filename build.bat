@echo off

set ENABLE_DOCUMENTATION=OFF
set ENABLE_SOURCE_BUILD=ON
set BUILD_TYPE=debug

REM Check if the first argument is provided
if /i "%1"=="debug" (
    if "%2"=="doc" set ENABLE_DOCUMENTATION=ON
) else if /i "%1"=="release" (
    set BUILD_TYPE=release
    if "%2"=="doc" set ENABLE_DOCUMENTATION=ON
) else if /i "%1"=="doc" (
    REM If the first argument is "doc", enable documentation and disable source build
    set ENABLE_SOURCE_BUILD=OFF
) else (
    echo "Usage1: build.bat [debug|release] <doc>"
    echo "If 'doc' is specified, documentation will be built."
    echo "If 'debug' or 'release' is specified, the source will be built."
    echo "Usage2: build.bat doc"
    echo "If 'doc' is specified, only documentation will be built without source."
    exit /b 1
)

set BUILD_DIR=build/%BUILD_TYPE%_win



if "%ENABLE_SOURCE_BUILD%"=="ON" (
    echo Starting source build...

    rem Create a build directory if it doesn't exist
    if not exist "%BUILD_DIR%" (
        mkdir "%BUILD_DIR%"
        if "%BUILD_TYPE%"=="debug" (
            cmake -S . -B "%BUILD_DIR%" -G "Ninja" -DIGESIO_BUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
        ) else (
            cmake -S . -B "%BUILD_DIR%" -G "Ninja" -DIGESIO_BUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
        )
    )

    rem Build the project using Ninja
    if "%ENABLE_DOCUMENTATION%"=="ON" (
        cmake --build "%BUILD_DIR%" --target doc
    ) else (
        cmake --build "%BUILD_DIR%"
    )
)
