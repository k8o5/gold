@echo off
color 0a
title Omega Powerful Batch

:menu
cls
echo ===================================
echo        OMEGA POWERFUL BATCH
echo ===================================
echo.
echo [1] System Cleanup
echo [2] Network Reset
echo [3] System Check
echo [4] Performance Boost
echo [5] Exit
echo.
set /p choice="Select an option (1-5): "

if "%choice%"=="1" goto cleanup
if "%choice%"=="2" goto network
if "%choice%"=="3" goto syscheck
if "%choice%"=="4" goto performance
if "%choice%"=="5" goto exit

:cleanup
cls
echo Running System Cleanup...
del /s /f /q %temp%\*.*
del /s /f /q C:\Windows\temp\*.*
cleanmgr /sagerun:1
echo Cleanup Complete!
pause
goto menu

:network
cls
echo Resetting Network...
ipconfig /release
ipconfig /renew
ipconfig /flushdns
netsh winsock reset
netsh int ip reset
echo Network Reset Complete!
pause
goto menu

:syscheck
cls
echo Running System Check...
sfc /scannow
DISM /Online /Cleanup-Image /CheckHealth
echo System Check Complete!
pause
goto menu

:performance
cls
echo Optimizing Performance...
net stop superfetch
net stop sysmain
powercfg -h off
echo Performance Optimization Complete!
pause
goto menu

:exit
cls
echo Thanks for using Omega Powerful Batch!
timeout /t 3
exit