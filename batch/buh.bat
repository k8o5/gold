@echo off
title Cyber Security Check
color 0A
echo [INFO] Starte Sicherheitsüberprüfung...
timeout /t 2 /nobreak >nul

:: Aktive Netzwerkverbindungen checken
echo [CHECK] Netzwerkverbindungen...
netstat -ano | findstr :80
netstat -ano | findstr :443

:: Laufende Prozesse anzeigen
echo [CHECK] Laufende Prozesse...
tasklist

:: Letzte fehlgeschlagene Anmeldeversuche auslesen
echo [CHECK] Letzte fehlgeschlagene Logins...
wmic nteventlog where (Logfile='Security') get Logfile, Sources | findstr /i "failed"

:: System-Firewall Status checken
echo [CHECK] Firewall Status...
netsh advfirewall show allprofiles

:: Offene Ports checken
echo [CHECK] Offene Ports...
netstat -an | find "LISTENING"

:: Liste aller Admin-User
echo [CHECK] Admin-Accounts...
net localgroup Administrators

echo [INFO] Sicherheitsüberprüfung abgeschlossen!
pause
