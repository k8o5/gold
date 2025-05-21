@echo off
setlocal enabledelayedexpansion

:: Startet den Shutdown-Countdown (3 Minuten)
echo Shutdown in 3 Minuten...
shutdown /s /t 180 /nobreak >nul

:: Versucht, so viele Rechner-Fenster wie möglich zu öffnen
:: aber mit einer Begrenzung, um das System nicht zu überlasten
set /a max_rechner=0
set /a schwellenwert=90  :: Prozessorauslastungsschwellenwert in %

:: Überprüft die aktuelle Prozessorauslastung und öffnet Rechner-Fenster
:loop
for /f "tokens=2 delims==" %%a in ('wmic cpu get loadpercentage /value') do (
    set /a cpu_auslastung=%%a
    if !cpu_auslastung! LSS %schwellenwert% (
        start /min "" calc.exe
        set /a max_rechner+=1
        >nul echo Öffne Rechner !max_rechner! (CPU-Auslastung: !cpu_auslastung!%)
        :: Kurze Pause, um das System nicht zu überfordern
        >nul timeout /t 1 /nobreak
        goto :loop
    ) else (
        >nul echo.
        >nul echo **Systembelastung erreicht** (CPU-Auslastung: !cpu_auslastung!%)
        >nul echo Insgesamt !max_rechner! Rechner-Fenster geöffnet.
        exit
    )
)