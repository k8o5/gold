@echo off
setlocal enabledelayedexpansion

:: Wartezeit in Sekunden (5 Minuten = 300 Sekunden)
set /a wait_time=300

:: Anzahl der Instanzen pro Anwendung
set /a instanz_count=1000

:: Z�hlvariable
set /a counter=0

:: Wartezeit einhalten, bevor das Chaos beginnt
echo Wartezeit von %wait_time% Sekunden, bevor %instanz_count% Rechner-Instanzen ge�ffnet werden...
timeout /t %wait_time% /nobreak >nul

:: �ffnen von vielen Instanzen des Rechners
:loop
if !counter! GEQ !instanz_count! goto :eof

:: Rechner
start calc.exe

:: Ein kurzer Pause, um das System nicht sofort �berlasten
timeout /t 1 /nobreak >nul

:: Z�hler erh�hen
set /a counter+=1
goto :loop

:eof
echo Fertig! %instanz_count% Rechner-Instanzen ge�ffnet.
pause >nul