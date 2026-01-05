# Check for Administrator privileges
if (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Warning "Script requires Administrator privileges to configure Firewall."
    Write-Warning "Please right-click and Run as Administrator."
    Read-Host "Press Enter to exit..."
    Break
}

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          Backend Server Setup (Windows Native)               ║"
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# 1. Configure Firewall
Write-Host "[Setup] Configuring Windows Firewall..." -ForegroundColor Yellow

# Allow UDP 9999 Outbound (Discovery)
$ruleOut = Get-NetFirewallRule -DisplayName "CafeManager Discovery Out" -ErrorAction SilentlyContinue
if (-not $ruleOut) {
    New-NetFirewallRule -DisplayName "CafeManager Discovery Out" -Direction Outbound -Protocol UDP -LocalPort 9999 -Action Allow
    Write-Host "  -> Allowed UDP 9999 (Outbound)" -ForegroundColor Green
} else {
    Write-Host "  -> UDP 9999 rule exists." -ForegroundColor Gray
}

# Allow TCP 9090 Inbound (Gateway Connection)
$ruleIn = Get-NetFirewallRule -DisplayName "CafeManager Backend In" -ErrorAction SilentlyContinue
if (-not $ruleIn) {
    New-NetFirewallRule -DisplayName "CafeManager Backend In" -Direction Inbound -Protocol TCP -LocalPort 9090 -Action Allow
    Write-Host "  -> Allowed TCP 9090 (Inbound)" -ForegroundColor Green
} else {
    Write-Host "  -> TCP 9090 rule exists." -ForegroundColor Gray
}

# 2. Build
Write-Host "[Setup] Building Backend..." -ForegroundColor Yellow

# Check for CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake not found! Please install CMake and add to PATH."
    Read-Host "Press Enter to exit..."
    Exit 1
}

# Create build directory
if (-not (Test-Path "build_win")) {
    New-Item -ItemType Directory -Path "build_win" | Out-Null
}

Push-Location "build_win"

# Configure and Build
cmake .. -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake Configuration failed."
    Read-Host "Press Enter to exit..."
    Pop-Location
    Exit 1
}

cmake --build . --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
    Read-Host "Press Enter to exit..."
    Pop-Location
    Exit 1
}

Pop-Location

# 3. Run
Write-Host "[Setup] Starting Backend Server..." -ForegroundColor Green
$binPath = ".\build_win\BackendServer.exe"
if (-not (Test-Path $binPath)) {
    $binPath = ".\build_win\Release\BackendServer.exe"
}

if (Test-Path $binPath) {
    Write-Host "[Info] Launching: $binPath 9090"
    & $binPath 9090
} else {
    Write-Error "Could not find BackendServer.exe in build_win."
    Read-Host "Press Enter to exit..."
    Exit 1
}

Write-Host "Backend stopped."
Read-Host "Press Enter to exit..."
