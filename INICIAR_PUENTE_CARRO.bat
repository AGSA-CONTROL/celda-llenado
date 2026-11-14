@echo off
REM Puente del carro fisico: el ESP32 se conecta a esta laptop (misma red WiFi, como antes)
REM y la laptop pasa sus mensajes al link de la nube. Necesita internet en la laptop.
cd /d "%~dp0"
taskkill /F /IM node.exe >nul 2>&1
if not exist node_modules\ws call npm install
set PUENTE_NUBE=wss://celda-llenado.onrender.com
node servidor_llenado_v17.js
pause
