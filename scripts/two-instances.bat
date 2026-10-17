@echo off
rem Starts two independent copies of ModernP2P on this PC so they can send files to each other.
rem Each copy has its own code, history and download folder (Downloads\ModernP2P-A / -B).
start "" "%~dp0ModernP2P.exe" --profile A
start "" "%~dp0ModernP2P.exe" --profile B
