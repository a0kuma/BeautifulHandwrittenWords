# 如果您使用 vcpkg，請使用這個版本的 CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(ImageViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用 vcpkg 時的套件查找
find_package(OpenCV REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

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
)

# 鏈接庫
target_link_libraries(ImageViewer 
    ${OpenCV_LIBS}
    glfw
    ${OPENGL_LIBRARIES}
)

# Windows 特定設置
if(WIN32)
    target_link_libraries(ImageViewer opengl32)
    # 如果需要控制台窗口，取消下面的註釋
    # set_target_properties(ImageViewer PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
