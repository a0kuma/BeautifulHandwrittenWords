# PowerShell script to install dependencies using vcpkg
Write-Host "Installing dependencies for Image Viewer..." -ForegroundColor Green

# Check if vcpkg is installed
$vcpkgPath = "C:\vcpkg"
if (-not (Test-Path $vcpkgPath)) {
    Write-Host "vcpkg not found. Installing vcpkg..." -ForegroundColor Yellow
    
    # Clone vcpkg
    git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    
    # Bootstrap vcpkg
    Set-Location C:\vcpkg
    .\bootstrap-vcpkg.bat
    
    # Integrate with Visual Studio
    .\vcpkg integrate install
} else {
    Write-Host "vcpkg found at $vcpkgPath" -ForegroundColor Green
    Set-Location $vcpkgPath
}

Write-Host "Installing OpenCV..." -ForegroundColor Yellow
.\vcpkg install opencv:x64-windows

Write-Host "Installing GLFW..." -ForegroundColor Yellow
.\vcpkg install glfw3:x64-windows

Write-Host "Installing OpenGL..." -ForegroundColor Yellow
.\vcpkg install opengl:x64-windows

Write-Host "All dependencies installed successfully!" -ForegroundColor Green
Write-Host "You can now build the full image viewer with:" -ForegroundColor Cyan
Write-Host "cd build" -ForegroundColor Cyan
Write-Host "cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -ForegroundColor Cyan
Write-Host "cmake --build . --config Release" -ForegroundColor Cyan
