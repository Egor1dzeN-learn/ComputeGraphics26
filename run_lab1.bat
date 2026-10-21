@echo off
chcp 65001 >nul
setlocal

rem Быстрая сборка и запуск lab_1_itk+vtk (VTK и ITK уже собраны setup_libs.bat).
rem Запуск: run_lab1.bat [стадия 0..4] [порог_кости_HU]
rem   run_lab1.bat          стадия 4 (скелет)
rem   run_lab1.bat 0        исходные данные
rem   run_lab1.bat 2 400    маска кости с порогом 400 HU
rem Библиотеки ищутся в папке libs рядом с этим файлом, иначе в D:\CompGraph\libs.

set "ROOT=%~dp0"
set "LAB=%ROOT%lab_1_itk+vtk"
set "LIBS=%ROOT%libs"
if not exist "%LIBS%\itk-build" set "LIBS=D:\CompGraph\libs"
if not exist "%LIBS%\itk-build" (echo Не найдены собранные библиотеки ни в %ROOT%libs, ни в D:\CompGraph\libs. Сначала запустите setup_libs.bat & goto :fail)
rem Для параметров CMake нужны прямые слеши
set "LIBS_CM=%LIBS:\=/%"

rem Если cmake нет в PATH, берём CMake из Visual Studio
where cmake >nul 2>&1 || call :find_cmake
where cmake >nul 2>&1 || (echo Не найден cmake & goto :fail)

rem Конфигурация только при первом запуске, дальше сборка инкрементальная
if not exist "%LAB%\build\CMakeCache.txt" (
  echo === Конфигурация, библиотеки: %LIBS%
  cmake -S "%LAB%" -B "%LAB%\build" -G "Visual Studio 17 2022" -A x64 -DLIBS_DIR=%LIBS_CM% || goto :fail
)

echo === Сборка
cmake --build "%LAB%\build" --config Release || goto :fail

echo === Запуск
cd /d "%LAB%"
build\Release\itk_vtk.exe data\ct %*
exit /b %ERRORLEVEL%

:find_cmake
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "PATH=%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
exit /b 0

:fail
echo.
echo ОШИБКА: см. сообщения выше.
pause
exit /b 1
