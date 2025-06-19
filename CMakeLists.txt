cmake_minimum_required(VERSION 3.16)
project(ImageViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 手動設置 vcpkg 路徑
set(VCPKG_ROOT "C:/vcpkg")
set(VCPKG_PACKAGES "${VCPKG_ROOT}/packages")

# OpenCV 路徑設置
set(OpenCV_DIR "${VCPKG_PACKAGES}/opencv4_x64-windows")
set(OpenCV_INCLUDE_DIRS 
    "${OpenCV_DIR}/include" 
    "${OpenCV_DIR}/include/opencv4"
)

# OpenCV 庫設置 - 只包含我們需要的核心模組
set(OpenCV_LIBS_RELEASE
    "${OpenCV_DIR}/lib/opencv_core4.lib"
    "${OpenCV_DIR}/lib/opencv_imgproc4.lib"
    "${OpenCV_DIR}/lib/opencv_imgcodecs4.lib"
    "${OpenCV_DIR}/lib/opencv_highgui4.lib"
)

set(OpenCV_LIBS_DEBUG
    "${OpenCV_DIR}/debug/lib/opencv_core4d.lib"
    "${OpenCV_DIR}/debug/lib/opencv_imgproc4d.lib"
    "${OpenCV_DIR}/debug/lib/opencv_imgcodecs4d.lib"
    "${OpenCV_DIR}/debug/lib/opencv_highgui4d.lib"
)

# GLFW 路徑設置
set(GLFW_DIR "${VCPKG_PACKAGES}/glfw3_x64-windows")
set(GLFW_INCLUDE_DIRS "${GLFW_DIR}/include")
set(GLFW_LIBRARIES_RELEASE "${GLFW_DIR}/lib/glfw3dll.lib")
set(GLFW_LIBRARIES_DEBUG "${GLFW_DIR}/debug/lib/glfw3dll.lib")

# GLEW 路徑設置
set(GLEW_DIR "${VCPKG_PACKAGES}/glew_x64-windows")
set(GLEW_INCLUDE_DIRS "${GLEW_DIR}/include")
set(GLEW_LIBRARIES_RELEASE "${GLEW_DIR}/lib/glew32.lib")
set(GLEW_LIBRARIES_DEBUG "${GLEW_DIR}/debug/lib/glew32d.lib")

# OpenGL 路徑設置
set(OPENGL_DIR "${VCPKG_PACKAGES}/opengl_x64-windows")
set(OPENGL_INCLUDE_DIRS "${OPENGL_DIR}/include")
set(OPENGL_LIBRARIES 
    "${OPENGL_DIR}/lib/OpenGL32.Lib"
    "${OPENGL_DIR}/lib/GlU32.Lib"
)

# libigl-stb 路徑設置
set(LIBIGL_STB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libigl-stb")

# ImGui 源文件
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# 創建可執行文件
add_executable(ImageViewer main.cpp ${IMGUI_SOURCES})

# 包含目錄
target_include_directories(ImageViewer PRIVATE 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${OpenCV_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
    ${LIBIGL_STB_DIR}
)

# 鏈接庫 - 使用生成器表達式選擇正確的庫
target_link_libraries(ImageViewer 
    $<$<CONFIG:Debug>:${OpenCV_LIBS_DEBUG}>
    $<$<CONFIG:Release>:${OpenCV_LIBS_RELEASE}>
    $<$<CONFIG:Debug>:${GLFW_LIBRARIES_DEBUG}>
    $<$<CONFIG:Release>:${GLFW_LIBRARIES_RELEASE}>
    $<$<CONFIG:Debug>:${GLEW_LIBRARIES_DEBUG}>
    $<$<CONFIG:Release>:${GLEW_LIBRARIES_RELEASE}>
    ${OPENGL_LIBRARIES}
)

# Windows 特定設置
if(WIN32)
    # Windows 系統庫
    target_link_libraries(ImageViewer 
        user32.lib
        gdi32.lib
        shell32.lib
    )
    # 如果需要控制台窗口，取消下面的註釋
    # set_target_properties(ImageViewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
