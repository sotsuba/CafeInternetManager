cd backend
if (!(Test-Path "build_win")) {
    mkdir build_win
}
cd build_win
cmake -G "MinGW Makefiles" ..
mingw32-make
Write-Host "Build Complete! functionality ECHO added."
