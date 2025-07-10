# 這是甚麼

基於 https://buttaiwan.github.io/writemyfont/ 製造了一個可以手寫並且變成字體的app，有很多人講說想要批次處理，有一個演算法我在做光達的時候寫出來的，然後目前可以用這個演算法去處理手寫的文字，但是還需要一些後處理，比如說，azure OCR 等等

## 技術的部分我想說的是

- 平行處理 畢竟會用到許多圖片
- 就是我寫光達的那個演算法
- OCR 的部分
- 排字的部分

## 阿那個時候說要用平行處理是... 

- 痾...我忘了

## 忘了的部分

- 我有寫GUI
- 這GUI實在是我沒有知道在幹嘛
- 如何編譯的?

---

# Image Viewer

A C++ image viewer application built with ImGui and OpenCV, featuring fullscreen display with thumbnail navigation.

## Features
- Fullscreen display mode
- Left panel showing thumbnails of all JPG images in `./impool` folder
- Right panel for main image viewing
- Support for JPG, JPEG, PNG, BMP formats
- Scrollable thumbnail list
- Click thumbnails to preview full-size images

## System Requirements
- Windows 10/11
- Visual Studio 2019 or newer
- CMake 3.16 or newer
- Git

## Quick Start

### Option 1: Automatic Installation (Recommended)
1. Open PowerShell as Administrator
2. Navigate to the project folder:
   ```powershell
   cd "c:\Users\ai\Documents\andy\code\learnPP"
   ```
3. Run the installation script:
   ```powershell
   Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
   .\install_dependencies.ps1
   ```

### Option 2: Manual Installation

#### Install vcpkg
```bash
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

#### Install Dependencies
```bash
.\vcpkg install opencv:x64-windows
.\vcpkg install glfw3:x64-windows
.\vcpkg install opengl:x64-windows
```

## Building the Project

1. Create build directory and configure:
   ```bash
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```

2. Build the project:
   ```bash
   cmake --build . --config Release
   ```

## Usage

1. Ensure the `impool` folder contains JPG images
2. Run `ImageViewer.exe` from the build/Release folder
3. The application will automatically load and display thumbnails
4. Click on thumbnails in the left panel to view full-size images

## Controls
- Left panel: Scrollable thumbnail view
- Click thumbnails to select images
- Selected thumbnail will have a yellow border
- Right panel shows the selected image scaled to fit
- Press ESC or close window to exit

## File Structure
```
learnPP/
├── main.cpp              # Main application source
├── test_basic.cpp        # Basic test without GUI dependencies
├── CMakeLists.txt        # Build configuration (vcpkg version)
├── CMakeLists_test.txt   # Test version build configuration
├── setup.bat             # Windows batch setup script
├── install_dependencies.ps1  # PowerShell installation script
├── README.md             # This file
├── imgui/                # ImGui library (downloaded by setup)
├── impool/               # Your image files go here
└── build/                # Build output directory
```

## Supported Image Formats
- JPG/JPEG
- PNG
- BMP

## Troubleshooting

### Build Errors
- Ensure all dependencies are properly installed via vcpkg
- Check that CMake can find OpenCV and GLFW
- Make sure you're using the correct vcpkg toolchain file path

### Runtime Errors
- Verify the `impool` folder exists and contains images
- Check that image files are not corrupted
- Ensure OpenCV and GLFW DLLs are in the system path or next to the executable

### Performance Issues
- Large numbers of images may take time to load thumbnails
- Thumbnails are loaded dynamically to save memory
- High-resolution images are automatically scaled for display

## Technical Details
- Built with C++17
- Uses OpenGL 3.3 for rendering
- ImGui for user interface
- OpenCV for image loading and processing
- GLFW for window management
- Supports Windows fullscreen mode
