// Simplified test version - only check code compilation
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
#endif

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#endif

namespace fs = std::filesystem;

int main() {    std::cout << "Image Viewer - Test Version" << std::endl;
    
    // Test file system functionality
    const std::string imageFolder = "./impool";
    
    if (!fs::exists(imageFolder)) {
        std::cout << "Error: Folder does not exist: " << imageFolder << std::endl;
        return -1;
    }
    
    std::vector<std::string> imageFiles;
    
    // List all image files
    for (const auto& entry : fs::directory_iterator(imageFolder)) {
        if (entry.is_regular_file()) {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            
            if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp") {
                imageFiles.push_back(entry.path().string());
                std::cout << "Found image: " << entry.path().filename().string() << std::endl;
            }
        }
    }
    
    std::cout << "Total found: " << imageFiles.size() << " images" << std::endl;    
#ifdef USE_OPENCV
    std::cout << "OpenCV enabled" << std::endl;
    if (!imageFiles.empty()) {
        cv::Mat testImage = cv::imread(imageFiles[0]);
        if (!testImage.empty()) {
            std::cout << "Successfully loaded first image: " << testImage.cols << "x" << testImage.rows << std::endl;
        }
    }
#else
    std::cout << "Note: OpenCV not enabled, please install OpenCV and recompile" << std::endl;
#endif

#ifdef USE_IMGUI    
    std::cout << "ImGui enabled" << std::endl;
#else
    std::cout << "Note: ImGui not enabled, please install GLFW and OpenGL dependencies and recompile" << std::endl;
#endif
    
    std::cout << "Test completed!" << std::endl;
    
    // Wait for user input
    std::cout << "Press Enter to exit...";
    std::cin.get();
    
    return 0;
}
