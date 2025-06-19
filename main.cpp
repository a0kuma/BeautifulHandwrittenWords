#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <cstdio>

#include <opencv2/opencv.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>

// Define OpenGL constants if not already defined
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace fs = std::filesystem;
using namespace std;

class ImageViewer {
public:
    struct ImageData {
        std::string filename;
        std::string filepath;
        cv::Mat image;
        cv::Mat thumbnail;
        GLuint textureID = 0;
        GLuint thumbnailTextureID = 0;
        bool textureLoaded = false;
        bool thumbnailLoaded = false;
    };

private:
    std::vector<ImageData> images;
    int selectedImageIndex = -1;
    const int thumbnailSize = 128;
    const std::string imageFolder = "C:\\Users\\ai\\Documents\\andy\\code\\learnPP\\impool";
    GLFWwindow* window;

    // OpenGL texture creation helper
    GLuint CreateTexture(const cv::Mat& image) {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        // Convert BGR to RGB
        cv::Mat rgbImage;
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
        
        // Upload texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 
                     0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
        
        return textureID;
    }

    // Create thumbnail
    cv::Mat CreateThumbnail(const cv::Mat& image) {
        cv::Mat thumbnail;
        double scale = std::min(static_cast<double>(thumbnailSize) / image.cols,
                               static_cast<double>(thumbnailSize) / image.rows);
        int newWidth = static_cast<int>(image.cols * scale);
        int newHeight = static_cast<int>(image.rows * scale);
        cv::resize(image, thumbnail, cv::Size(newWidth, newHeight));
        return thumbnail;
    }

    // Load all images
    void LoadImageList() {
        images.clear();
        
        if (!fs::exists(imageFolder)) {
            std::cout << "Folder does not exist: " << imageFolder << std::endl;
            return;
        }

        for (const auto& entry : fs::directory_iterator(imageFolder)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp") {
                    ImageData imgData;
                    imgData.filename = entry.path().filename().string();
                    imgData.filepath = entry.path().string();
                    images.push_back(imgData);
                }
            }
        }
        
        std::cout << "Found " << images.size() << " images" << std::endl;
    }

    // Load image texture
    void LoadImageTexture(ImageData& imgData) {
        if (imgData.textureLoaded) return;
        
        imgData.image = cv::imread(imgData.filepath);
        if (imgData.image.empty()) {
            std::cout << "Cannot load image: " << imgData.filepath << std::endl;
            return;
        }
        
        imgData.textureID = CreateTexture(imgData.image);
        imgData.textureLoaded = true;
    }

    // Load thumbnail texture
    void LoadThumbnailTexture(ImageData& imgData) {
        if (imgData.thumbnailLoaded) return;
        
        if (imgData.image.empty()) {
            imgData.image = cv::imread(imgData.filepath);
            if (imgData.image.empty()) return;
        }
        
        imgData.thumbnail = CreateThumbnail(imgData.image);
        imgData.thumbnailTextureID = CreateTexture(imgData.thumbnail);
        imgData.thumbnailLoaded = true;
    }

public:
    ImageViewer() : window(nullptr) {}
    
    ~ImageViewer() {
        // Clean up textures
        for (auto& img : images) {
            if (img.textureLoaded) glDeleteTextures(1, &img.textureID);
            if (img.thumbnailLoaded) glDeleteTextures(1, &img.thumbnailTextureID);
        }
    }    bool Initialize() {
        std::cout << "Initializing GLFW..." << std::endl;
        
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        std::cout << "Setting OpenGL hints..." << std::endl;
        
        // Set OpenGL version - try more compatible settings
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);

        std::cout << "Creating window..." << std::endl;
        
        // Create windowed mode first (easier to debug)
        window = glfwCreateWindow(1200, 800, "Image Viewer", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create window" << std::endl;
            const char* description;
            int code = glfwGetError(&description);
            if (description) {
                std::cerr << "GLFW Error: " << code << " - " << description << std::endl;
            }
            glfwTerminate();
            return false;
        }

        std::cout << "Making context current..." << std::endl;
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Vertical sync

        std::cout << "Initializing ImGui..." << std::endl;
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Set ImGui style
        ImGui::StyleColorsDark();

        std::cout << "Initializing ImGui backends..." << std::endl;
        
        // Initialize ImGui backends
        if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
            std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
            return false;
        }
        
        if (!ImGui_ImplOpenGL3_Init("#version 330")) {
            std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
            return false;
        }

        std::cout << "Loading image list..." << std::endl;
        
        // Load image list
        LoadImageList();

        std::cout << "Initialization complete!" << std::endl;
        return true;
    }void Run() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Get window size
            int displayWidth, displayHeight;
            glfwGetFramebufferSize(window, &displayWidth, &displayHeight);

            // Create main window
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(displayWidth, displayHeight));
            
            ImGui::Begin("Image Viewer", nullptr, 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoCollapse);

            // Left thumbnail panel
            ImGui::BeginChild("Thumbnail Panel", ImVec2(300, 0), true);
            
            for (int i = 0; i < images.size(); i++) {
                auto& img = images[i];
                
                // Load thumbnail
                if (!img.thumbnailLoaded) {
                    LoadThumbnailTexture(img);
                }
                  if (img.thumbnailLoaded) {
                    // Display thumbnail
                    char buttonId[64];
                    snprintf(buttonId, sizeof(buttonId), "thumbnail_%d", i);
                    if (ImGui::ImageButton(buttonId, reinterpret_cast<void*>(img.thumbnailTextureID), 
                                         ImVec2(thumbnailSize, thumbnailSize))) {
                        selectedImageIndex = i;
                    }
                    
                    // Highlight selection
                    if (selectedImageIndex == i) {
                        ImVec2 rectMin = ImGui::GetItemRectMin();
                        ImVec2 rectMax = ImGui::GetItemRectMax();
                        ImGui::GetWindowDrawList()->AddRect(rectMin, rectMax, 
                                                          IM_COL32(255, 255, 0, 255), 0.0f, 0, 3.0f);
                    }
                } else {
                    // Display loading placeholder
                    ImGui::Button("Loading...", ImVec2(thumbnailSize, thumbnailSize));
                    if (ImGui::IsItemClicked()) {
                        selectedImageIndex = i;
                    }
                }
                
                // Display filename
                ImGui::Text("%s", img.filename.c_str());
                ImGui::Separator();
            }
            
            ImGui::EndChild();

            ImGui::SameLine();

            // Right image display panel
            ImGui::BeginChild("Image Display", ImVec2(0, 0), true);
            
            if (selectedImageIndex >= 0 && selectedImageIndex < images.size()) {
                auto& selectedImg = images[selectedImageIndex];
                
                // Load full image
                if (!selectedImg.textureLoaded) {
                    LoadImageTexture(selectedImg);
                }
                
                if (selectedImg.textureLoaded && !selectedImg.image.empty()) {
                    // Calculate display size to fit panel
                    ImVec2 panelSize = ImGui::GetContentRegionAvail();
                    float imgAspect = static_cast<float>(selectedImg.image.cols) / selectedImg.image.rows;
                    float panelAspect = panelSize.x / panelSize.y;
                    
                    ImVec2 imageSize;
                    if (imgAspect > panelAspect) {
                        imageSize.x = panelSize.x;
                        imageSize.y = panelSize.x / imgAspect;
                    } else {
                        imageSize.y = panelSize.y;
                        imageSize.x = panelSize.y * imgAspect;
                    }
                    
                    // Center display
                    ImVec2 cursorPos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(
                        cursorPos.x + (panelSize.x - imageSize.x) * 0.5f,
                        cursorPos.y + (panelSize.y - imageSize.y) * 0.5f
                    ));
                    
                    ImGui::Image(reinterpret_cast<void*>(selectedImg.textureID), imageSize);
                    
                    // Display image info
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("File: %s", selectedImg.filename.c_str());
                    ImGui::Text("Size: %dx%d", selectedImg.image.cols, selectedImg.image.rows);
                }
            } else {
                // Display prompt
                ImVec2 textSize = ImGui::CalcTextSize("Please select an image from the left panel");
                ImVec2 panelSize = ImGui::GetContentRegionAvail();
                ImGui::SetCursorPos(ImVec2(
                    (panelSize.x - textSize.x) * 0.5f,
                    (panelSize.y - textSize.y) * 0.5f
                ));
                ImGui::Text("Please select an image from the left panel");
            }
            
            ImGui::EndChild();

            ImGui::End();

            // Render
            glViewport(0, 0, displayWidth, displayHeight);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }
    }

    void Cleanup() {
        // Clean up ImGui
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        // Clean up GLFW
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }
};

int main() {
    cout<< "Image Viewer Application" << endl;
    ImageViewer viewer;
    cout<< "debug point1" << endl;
    
    if (!viewer.Initialize()) {
        std::cerr << "Initialization failed" << std::endl;
        return -1;
    }
    
    cout<< "debug point2" << endl;
    viewer.Run();
    cout<< "debug point3" << endl;
    viewer.Cleanup();
    cout<< "debug point4" << endl;
    
    return 0;
}