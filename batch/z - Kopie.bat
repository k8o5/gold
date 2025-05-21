@echo off
color 0a
title OMEGA HACKER 9000

:start
cls
echo OMEGA HACKER 9000
echo ================
echo.
echo [1] MATRIX RAIN
echo [2] SYSTEM SCAN
echo [3] MEGA HACK
echo [4] NUKE PC
echo [5] EXIT
echo.
set /p wahl="COMMAND: "

if "%wahl%"=="1" goto matrix
if "%wahl%"=="2" goto scan
if "%wahl%"=="3" goto hack
if "%wahl%"=="4" goto nuke
if "%wahl%"=="5" exit

:matrix
cls
:matrixloop
echo %random%%random%%random%%random%%random%%random%%random%%random%
ping -n 1 localhost >nul
goto matrixloop

:scan
cls
echo SCANNING SYSTEM...
echo.
echo CPU: %PROCESSOR_IDENTIFIER%
echo RAM: %MEMORY_LIMIT%
echo OS:  %OS%
echo USER: %USERNAME%
echo.
echo SCAN COMPLETE!
pause
goto start

:hack
cls
:hackloop
echo ACCESS_MAINFRAME... %random% BREACH_FIREWALL... %random% DOWNLOAD_DATA... %random%
ping -n 1 localhost >nul
goto hackloop

:nuke
cls
echo WARNING! SYSTEM WILL BE DESTROYED IN:
echo.
echo 5
ping -n 2 localhost >nul
cls
echo 4
ping -n 2 localhost >nul
cls
echo 3
ping -n 2 localhost >nul
cls
echo 2
ping -n 2 localhost >nul
cls
echo 1
ping -n 2 localhost >nul
cls
echo BOOM!
ping -n 2 localhost >nul
goto start