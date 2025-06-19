@echo off
echo 正在設置圖片檢視器項目...

REM 克隆 ImGui
if not exist "imgui" (
    echo 正在下載 ImGui...
    git clone https://github.com/ocornut/imgui.git
) else (
    echo ImGui 已存在
)

REM 創建 build 資料夾
if not exist "build" (
    mkdir build
)

echo.
echo 請確保您已安裝:
echo 1. CMake (https://cmake.org/download/)
echo 2. OpenCV (可通過 vcpkg 安裝: vcpkg install opencv)
echo 3. GLFW (可通過 vcpkg 安裝: vcpkg install glfw3)
echo 4. Visual Studio (包含 C++ 工具)
echo.
echo 如果使用 vcpkg，請運行:
echo vcpkg install opencv glfw3 opengl
echo.
echo 然後運行以下命令來建置專案:
echo cd build
echo cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg路徑]/scripts/buildsystems/vcpkg.cmake
echo cmake --build . --config Release
echo.
pause
