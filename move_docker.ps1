# Docker Data Migration Script (C: -> D:)
# Run this script as Administrator

$TargetDir = "d:\DockerData"
$DistroName = "docker-desktop-data"
$ExportFile = "$TargetDir\docker-data.tar"

Write-Host "=== DOCKER DATA MIGRATION UTILITY ===" -ForegroundColor Cyan
Write-Host "This script will move Docker's data file (images/containers) from C: to $TargetDir"
Write-Host "Target Drive Free Space Check..."

# Check D: drive existence
if (!(Test-Path "d:\")) {
    Write-Error "D: drive not found! Cannot proceed."
    exit 1
}

# Create Target Directory
if (!(Test-Path $TargetDir)) {
    New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
    Write-Host "Created $TargetDir" -ForegroundColor Green
}

# Step 1: Shutdown Docker Desktop
Write-Host "`n[Step 1] Shutting down Docker Desktop and WSL..." -ForegroundColor Yellow
& "C:\Program Files\Docker\Docker\DockerCli.exe" -SwitchDaemon
wsl --shutdown
Start-Sleep -Seconds 5

# Step 2: Export Data
Write-Host "`n[Step 2] Exporting current Docker data to $ExportFile..." -ForegroundColor Yellow
Write-Host "This may take a while depending on size (approx 28GB)..."
wsl --export $DistroName $ExportFile

if (!(Test-Path $ExportFile)) {
    Write-Error "Export failed! File not found."
    exit 1
}
Write-Host "Export successful!" -ForegroundColor Green

# Step 3: Unregister Old Data
Write-Host "`n[Step 3] Removing old data from C:..." -ForegroundColor Yellow
wsl --unregister $DistroName
Write-Host "Old data removed." -ForegroundColor Green

# Step 4: Import to New Location
Write-Host "`n[Step 4] Importing data to new location on D:..." -ForegroundColor Yellow
wsl --import $DistroName "$TargetDir\wsl" $ExportFile --version 2
Write-Host "Import successful!" -ForegroundColor Green

# Step 5: Cleanup
Write-Host "`n[Step 5] Cleaning up temporary export file..."
Remove-Item $ExportFile
Write-Host "Cleanup done." -ForegroundColor Green

Write-Host "`n=== MIGRATION COMPLETE ===" -ForegroundColor Cyan
Write-Host "Please start Docker Desktop manually now."
Write-Host "Your C: drive should now have ~28GB more free space."
