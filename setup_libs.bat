@echo off
chcp 65001 >nul
setlocal

rem Скачивает и собирает VTK 9.3.1, ITK 5.4.0 (с ITKVtkGlue) и примеры FenixFly/VTK_tutorial.
rem Запуск: setup_libs.bat [папка]   (по умолчанию D:\CompGraph\libs; путь без кириллицы и пробелов)
rem Нужны в PATH: git, cmake. Нужна Visual Studio 2022 с C++.

set "LIBS=%~1"
if "%LIBS%"=="" set "LIBS=D:\CompGraph\libs"
rem Для параметров CMake нужны прямые слеши
set "LIBS_CM=%LIBS:\=/%"
set GEN=-G "Visual Studio 17 2022" -A x64 -Wno-dev
set BUILD=--config Release --parallel

where git >nul 2>&1 || (echo Не найден git & goto :fail)
where cmake >nul 2>&1 || (echo Не найден cmake & goto :fail)
if not exist "%LIBS%" mkdir "%LIBS%"
echo Библиотеки: %LIBS%

echo.
echo === [1/6] Клонирование
if not exist "%LIBS%\vtk-src" git clone --depth 1 --branch v9.3.1 https://gitlab.kitware.com/vtk/vtk.git "%LIBS%\vtk-src" || goto :fail
if not exist "%LIBS%\itk-src" git clone --depth 1 --branch v5.4.0 https://github.com/InsightSoftwareConsortium/ITK.git "%LIBS%\itk-src" || goto :fail
if not exist "%LIBS%\VTK_tutorial" git clone --depth 1 https://github.com/FenixFly/VTK_tutorial.git "%LIBS%\VTK_tutorial" || goto :fail

echo.
echo === [2/6] Правка CMakeLists.txt примеров под VTK 9
set "TUT=%LIBS%\VTK_tutorial\CMakeLists.txt"
findstr /c:"VTK_USE_FILE" "%TUT%" >nul || goto :patched
findstr /v /c:"VTK_USE_FILE" "%TUT%" > "%TUT%.new"
move /y "%TUT%.new" "%TUT%" >nul
>>"%TUT%" echo.
>>"%TUT%" echo get_directory_property(targets BUILDSYSTEM_TARGETS)
>>"%TUT%" echo vtk_module_autoinit(TARGETS ${targets} MODULES ${VTK_LIBRARIES})
:patched

echo.
echo === [3/6] VTK: конфигурация
cmake -S "%LIBS%\vtk-src" -B "%LIBS%\vtk-build" %GEN% -DBUILD_SHARED_LIBS=ON -DVTK_BUILD_TESTING=OFF || goto :fail
echo === [4/6] VTK: сборка (~30-40 мин)
cmake --build "%LIBS%\vtk-build" %BUILD% || goto :fail

echo.
echo === [5/6] ITK: конфигурация и сборка (~30 мин)
cmake -S "%LIBS%\itk-src" -B "%LIBS%\itk-build" %GEN% -DModule_ITKVtkGlue=ON -DVTK_DIR=%LIBS_CM%/vtk-build -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF || goto :fail
cmake --build "%LIBS%\itk-build" %BUILD% || goto :fail

echo.
echo === [6/6] Примеры VTK_tutorial
cmake -S "%LIBS%\VTK_tutorial" -B "%LIBS%\VTK_tutorial\build" %GEN% -DVTK_DIR=%LIBS_CM%/vtk-build || goto :fail
cmake --build "%LIBS%\VTK_tutorial\build" %BUILD% || goto :fail

echo.
echo Готово. Проверка VTK:
echo   set PATH=%LIBS%\vtk-build\bin\Release;%%PATH%%
echo   "%LIBS%\VTK_tutorial\build\Release\VolumeRender.exe" "%LIBS%\VTK_tutorial\data\head.vti"
if not "%LIBS_CM%"=="D:/CompGraph/libs" echo Для лабораторных: cmake ... -DLIBS_DIR=%LIBS_CM%
pause
exit /b 0

:fail
echo.
echo ОШИБКА: см. сообщения выше.
pause
exit /b 1
