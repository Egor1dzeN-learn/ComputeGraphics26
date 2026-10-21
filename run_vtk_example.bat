@echo off
chcp 65001 >nul
setlocal

rem Запуск примеров FenixFly/VTK_tutorial (собираются setup_libs.bat).
rem Запуск: run_vtk_example.bat [пример] [файл_данных]
rem   run_vtk_example.bat                     VolumeRender с head.vti
rem   run_vtk_example.bat ContourFilter       другой пример с подходящими данными
rem   run_vtk_example.bat list                список собранных примеров
rem Библиотеки ищутся в папке libs рядом с этим файлом, иначе в D:\CompGraph\libs.

set "LIBS=%~dp0libs"
if not exist "%LIBS%\VTK_tutorial\build" set "LIBS=D:\CompGraph\libs"
set "BIN=%LIBS%\VTK_tutorial\build\Release"
set "DATA=%LIBS%\VTK_tutorial\data"
rem Примеры не копируют DLL VTK к себе, поэтому добавляем их в PATH
set "PATH=%LIBS%\vtk-build\bin\Release;%PATH%"

if not exist "%BIN%" (echo Примеры не собраны ни в %~dp0libs, ни в D:\CompGraph\libs. Сначала запустите setup_libs.bat & goto :fail)

set "NAME=%~1"
if "%NAME%"=="" set "NAME=VolumeRender"
if /i "%NAME%"=="list" (
  echo Собранные примеры:
  for %%f in ("%BIN%\*.exe") do echo   %%~nf
  exit /b 0
)
if not exist "%BIN%\%NAME%.exe" (echo Нет примера %NAME%. Список: run_vtk_example.bat list & goto :fail)

rem Данные по умолчанию: ObjReader читает .obj, First - first.vti, остальные с аргументом - head.vti
set "FILE=%~2"
if "%FILE%"=="" (
  if /i "%NAME%"=="ObjReader" (set "FILE=%DATA%\Heart.obj") else if /i "%NAME%"=="First" (set "FILE=%DATA%\first.vti") else (set "FILE=%DATA%\head.vti")
)

echo %NAME% "%FILE%"
cd /d "%BIN%"
"%BIN%\%NAME%.exe" "%FILE%"
exit /b %ERRORLEVEL%

:fail
echo.
echo ОШИБКА: см. сообщения выше.
pause
exit /b 1
