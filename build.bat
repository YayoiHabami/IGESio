@echo off

set ENABLE_DOCUMENTATION=OFF
set ENABLE_SOURCE_BUILD=ON
set BUILD_TYPE=debug
set BUILD_EXAMPLE=OFF
set COMPILER=gcc

REM Check if the first argument is provided
if /i "%1"=="debug" (
    if /i "%2"=="doc" set ENABLE_DOCUMENTATION=ON
    if /i "%2"=="ex" set BUILD_EXAMPLE=ON
    if /i "%2"=="clang" set COMPILER=clang
    if /i "%3"=="ex" set BUILD_EXAMPLE=ON
    if /i "%3"=="clang" set COMPILER=clang
    if /i "%4"=="clang" set COMPILER=clang
) else if /i "%1"=="release" (
    set BUILD_TYPE=release
    if /i "%2"=="doc" set ENABLE_DOCUMENTATION=ON
    if /i "%2"=="ex" set BUILD_EXAMPLE=ON
    if /i "%2"=="clang" set COMPILER=clang
    if /i "%3"=="ex" set BUILD_EXAMPLE=ON
    if /i "%3"=="clang" set COMPILER=clang
    if /i "%4"=="clang" set COMPILER=clang
) else if /i "%1"=="doc" (
    REM If the first argument is "doc", enable documentation and disable source build
    set ENABLE_SOURCE_BUILD=OFF
) else (
    echo "Usage1: build.bat [debug|release] <doc> <ex> <gcc|clang>"
    echo "If 'doc' is specified, documentation will be built."
    echo "If 'ex' is specified, example will be built."
    echo "If 'debug' or 'release' is specified, the source will be built."
    echo "Usage2: build.bat doc <ex>"
    echo "If 'doc' is specified, only documentation will be built without source."
    exit /b 1
)

if "%BUILD_EXAMPLE%"=="ON" (
    set EXAMPLE_SUFFIX=_ex
) else (
    set EXAMPLE_SUFFIX=
)
set BUILD_DIR=build/%BUILD_TYPE%%EXAMPLE_SUFFIX%_win
if not "%COMPILER%"=="gcc" (
    set BUILD_DIR=%BUILD_DIR%_%COMPILER%
    echo Using compiler: %COMPILER%
)



if "%ENABLE_SOURCE_BUILD%"=="ON" (
    echo Starting source build...

    rem Create a build directory if it doesn't exist
    if not exist "%BUILD_DIR%" (
        mkdir "%BUILD_DIR%"

        if "%COMPILER%"=="gcc" (
            set COMPILER_OPTIONS="-DCMAKE_C_COMPILER=gcc" "-DCMAKE_CXX_COMPILER=g++"
        ) else if "%COMPILER%"=="clang" (
            set COMPILER_OPTIONS="-DCMAKE_C_COMPILER=clang" "-DCMAKE_CXX_COMPILER=clang++"
        ) else (
            echo "Unsupported compiler: %COMPILER%"
            exit /b 1
        )

        if "%BUILD_TYPE%"=="debug" (
            set BUILD_TYPE_OPTION="-DCMAKE_BUILD_TYPE=Debug"
            set BUILD_TESTING_OPTION="-DIGESIO_BUILD_TESTING=ON"
        ) else (
            set BUILD_TYPE_OPTION="-DCMAKE_BUILD_TYPE=Release"
            set BUILD_TESTING_OPTION="-DIGESIO_BUILD_TESTING=OFF"
        )
        if "%BUILD_EXAMPLE%"=="ON" (
            rem If the example is enabled, OpenGL and GUI are also enabled
            set EXAMPLE_OPTION="-DIGESIO_BUILD_EXAMPLE=ON" "-DIGESIO_ENABLE_GRAPHICS=ON" "-DIGESIO_BUILD_GUI=ON"
        ) else (
            set EXAMPLE_OPTION="-DIGESIO_BUILD_EXAMPLE=OFF"
        )

        echo -S . -B "%BUILD_DIR%" -G "Ninja" %COMPILER_OPTIONS% %BUILD_TESTING_OPTION% %BUILD_TYPE_OPTION% %EXAMPLE_OPTION%
        cmake -S . -B "%BUILD_DIR%" -G "Ninja" %COMPILER_OPTIONS% %BUILD_TESTING_OPTION% %BUILD_TYPE_OPTION% %EXAMPLE_OPTION%
    )

    set DOCUMENT_OPTION=
    if "%ENABLE_DOCUMENTATION%"=="ON" (
        set DOCUMENT_OPTION="--target doc"
    )

    rem Build the project using Ninja
    cmake --build "%BUILD_DIR%" %DOCUMENT_OPTION%
)
