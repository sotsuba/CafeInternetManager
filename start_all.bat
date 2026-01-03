@echo off
echo ==========================================
echo   Cafe Internet Manager - Startup Script
echo ==========================================

echo.
echo [1/3] Starting Gateway (Docker)...
docker-compose up -d gateway
if %errorlevel% neq 0 (
    echo [ERROR] Failed to start Docker Gateway. Make sure Docker Desktop is running.
    pause
    exit /b
)

echo.
echo [2/3] Starting Backend Server...
:: Starts in a new separate window
start "Backend Server (Core)" cmd /k "cd backend && build\Release\BackendServer.exe"

echo.
echo [3/3] Starting Frontend Dashboard...
:: Starts in a new separate window
start "Frontend (Dashboard)" cmd /k "cd frontend && npm run dev"

echo.
echo [SUCCESS] All services launched!
echo - Frontend: http://localhost:5173
echo - Backend:  TCP 9090 (Background)
echo - Gateway:  localhost:8881
echo.
pause
