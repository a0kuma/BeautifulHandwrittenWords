/**
 * !important!
 * cd build; cmake .. -G "Visual Studio 17 2022" -A x64 ; cmake --build . --config Release; .\Release\ImageViewer.exe 
 */

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

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
    GLFWwindow* window;    // OpenGL texture creation helper
    GLuint CreateTexture(const cv::Mat& image) {
        std::cout << "[DEBUG] CreateTexture called" << std::endl;
        
        if (image.empty()) {
            std::cerr << "[ERROR] Cannot create texture from empty image" << std::endl;
            return 0;
        }
        
        std::cout << "[DEBUG] Image info - Size: " << image.cols << "x" << image.rows 
                  << ", Channels: " << image.channels() << ", Type: " << image.type() << std::endl;
        
        // Check if image is too large
        if (image.cols > 4096 || image.rows > 4096) {
            std::cerr << "[WARNING] Image is very large: " << image.cols << "x" << image.rows << std::endl;
        }
        
        GLuint textureID;
        glGenTextures(1, &textureID);
        if (textureID == 0) {
            std::cerr << "[ERROR] Failed to generate texture ID" << std::endl;
            return 0;
        }
        
        std::cout << "[DEBUG] Generated texture ID: " << textureID << std::endl;
        
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // Check for OpenGL errors after binding
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "[ERROR] OpenGL error after binding texture: " << error << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        // Check for OpenGL errors after setting parameters
        error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "[ERROR] OpenGL error after setting texture parameters: " << error << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
          // Convert BGR to RGB
        cv::Mat rgbImage;
        try {
            // Additional validation before color conversion
            if (image.empty() || image.data == nullptr) {
                std::cerr << "[ERROR] Input image is empty or has null data before conversion" << std::endl;
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            // Check image data integrity
            if (image.total() == 0) {
                std::cerr << "[ERROR] Image has zero total elements" << std::endl;
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            std::cout << "[DEBUG] Converting image with " << image.channels() << " channels, size: " 
                      << image.cols << "x" << image.rows << ", type: " << image.type() << std::endl;
            
            if (image.channels() == 3) {
                cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
                std::cout << "[DEBUG] Converted from BGR to RGB" << std::endl;
            } else if (image.channels() == 4) {
                cv::cvtColor(image, rgbImage, cv::COLOR_BGRA2RGB);
                std::cout << "[DEBUG] Converted from BGRA to RGB" << std::endl;
            } else if (image.channels() == 1) {
                cv::cvtColor(image, rgbImage, cv::COLOR_GRAY2RGB);
                std::cout << "[DEBUG] Converted from GRAY to RGB" << std::endl;
            } else {
                std::cerr << "[ERROR] Unsupported image format with " << image.channels() << " channels" << std::endl;
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            // Validate conversion result
            if (rgbImage.empty() || rgbImage.data == nullptr) {
                std::cerr << "[ERROR] Color conversion failed - result is empty" << std::endl;
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
        } catch (const cv::Exception& e) {
            std::cerr << "[ERROR] OpenCV error during color conversion: " << e.what() << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Standard exception during color conversion: " << e.what() << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (...) {
            std::cerr << "[ERROR] Unknown exception during color conversion" << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
          // Validate converted image
        if (rgbImage.empty() || rgbImage.data == nullptr) {
            std::cerr << "[ERROR] RGB image is empty or has null data after conversion" << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        std::cout << "[DEBUG] RGB image size: " << rgbImage.cols << "x" << rgbImage.rows << std::endl;
        
        // Additional validation checks
        if (rgbImage.cols <= 0 || rgbImage.rows <= 0) {
            std::cerr << "[ERROR] Invalid image dimensions: " << rgbImage.cols << "x" << rgbImage.rows << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Check if image is too large for OpenGL
        GLint maxTextureSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        std::cout << "[DEBUG] Max OpenGL texture size: " << maxTextureSize << std::endl;
        
        if (rgbImage.cols > maxTextureSize || rgbImage.rows > maxTextureSize) {
            std::cerr << "[ERROR] Image too large for OpenGL: " << rgbImage.cols << "x" << rgbImage.rows 
                      << " (max: " << maxTextureSize << ")" << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Validate image data integrity
        int expectedDataSize = rgbImage.cols * rgbImage.rows * 3; // 3 channels for RGB
        if (rgbImage.total() * rgbImage.elemSize() != expectedDataSize) {
            std::cerr << "[ERROR] Image data size mismatch. Expected: " << expectedDataSize 
                      << ", Actual: " << (rgbImage.total() * rgbImage.elemSize()) << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Check if image data is continuous in memory
        if (!rgbImage.isContinuous()) {
            std::cout << "[DEBUG] Image is not continuous, creating continuous copy" << std::endl;
            cv::Mat continuousImage = rgbImage.clone();
            rgbImage = continuousImage;
        }
        
        std::cout << "[DEBUG] Image validation passed, uploading texture data..." << std::endl;
        
        // Upload texture data
        try {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 
                         0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
            
            // Check for OpenGL errors immediately after texture upload
            GLenum uploadError = glGetError();
            if (uploadError != GL_NO_ERROR) {
                std::cerr << "[ERROR] OpenGL error during texture upload: " << uploadError << std::endl;
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            std::cout << "[DEBUG] Texture data uploaded successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Standard exception during glTexImage2D: " << e.what() << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (...) {
            std::cerr << "[ERROR] Unknown exception during glTexImage2D" << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Check for OpenGL errors
        error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "[ERROR] OpenGL error creating texture: " << error << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        std::cout << "[DEBUG] Texture created successfully with ID: " << textureID << std::endl;
        return textureID;
    }

    // Create thumbnail
    cv::Mat CreateThumbnail(const cv::Mat& image) {
        std::cout << "[DEBUG] CreateThumbnail called for image " << image.cols << "x" << image.rows << std::endl;
        
        if (image.empty()) {
            std::cerr << "[ERROR] Cannot create thumbnail from empty image" << std::endl;
            return cv::Mat();
        }
        
        cv::Mat thumbnail;
        try {
            double scale = std::min(static_cast<double>(thumbnailSize) / image.cols,
                                   static_cast<double>(thumbnailSize) / image.rows);
            int newWidth = static_cast<int>(image.cols * scale);
            int newHeight = static_cast<int>(image.rows * scale);
            
            std::cout << "[DEBUG] Thumbnail scale: " << scale << ", new size: " << newWidth << "x" << newHeight << std::endl;
            
            if (newWidth <= 0 || newHeight <= 0) {
                std::cerr << "[ERROR] Invalid thumbnail dimensions: " << newWidth << "x" << newHeight << std::endl;
                return cv::Mat();
            }
            
            cv::resize(image, thumbnail, cv::Size(newWidth, newHeight));
            std::cout << "[DEBUG] Thumbnail created successfully" << std::endl;
        } catch (const cv::Exception& e) {
            std::cerr << "[ERROR] OpenCV error during thumbnail creation: " << e.what() << std::endl;
            return cv::Mat();
        } catch (...) {
            std::cerr << "[ERROR] Unknown error during thumbnail creation" << std::endl;
            return cv::Mat();
        }
        
        return thumbnail;
    }    // Load all images
    void LoadImageList() {
        std::cout << "[DEBUG] LoadImageList called" << std::endl;
        images.clear();
        
        std::cout << "Checking image folder: " << imageFolder << std::endl;
        
        if (!fs::exists(imageFolder)) {
            std::cout << "Folder does not exist: " << imageFolder << std::endl;
            std::cout << "Creating the folder..." << std::endl;
            try {
                fs::create_directories(imageFolder);
            } catch (const std::exception& e) {
                std::cerr << "Failed to create directory: " << e.what() << std::endl;
                return;
            }
        }

        try {
            int imageCount = 0;
            for (const auto& entry : fs::directory_iterator(imageFolder)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                    
                    if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp") {
                        ImageData imgData;
                        imgData.filename = entry.path().filename().string();
                        imgData.filepath = entry.path().string();
                        
                        // Basic file validation
                        try {
                            auto fileSize = fs::file_size(entry.path());
                            std::cout << "[DEBUG] Found image: " << imgData.filename 
                                      << " (size: " << fileSize << " bytes)" << std::endl;
                            
                            if (fileSize == 0) {
                                std::cerr << "[WARNING] Skipping empty file: " << imgData.filename << std::endl;
                                continue;
                            }
                            
                            if (fileSize > 100 * 1024 * 1024) { // 100MB limit
                                std::cerr << "[WARNING] Very large file: " << imgData.filename 
                                          << " (" << fileSize << " bytes)" << std::endl;
                            }
                            
                        } catch (const std::exception& e) {
                            std::cerr << "[WARNING] Cannot get file size for: " << imgData.filename 
                                      << " - " << e.what() << std::endl;
                        }
                        
                        images.push_back(imgData);
                        imageCount++;
                        
                        // Limit number of images to prevent memory issues
                        if (imageCount >= 1000) {
                            std::cout << "[WARNING] Reached maximum image limit (1000), stopping scan" << std::endl;
                            break;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error reading directory: " << e.what() << std::endl;
        }
        
        std::cout << "Found " << images.size() << " images" << std::endl;
    }

    // Load image texture
    void LoadImageTexture(ImageData& imgData) {
        std::cout << "[DEBUG] LoadImageTexture called for: " << imgData.filename << std::endl;
        
        if (imgData.textureLoaded) {
            std::cout << "[DEBUG] Texture already loaded, skipping" << std::endl;
            return;
        }
        
        std::cout << "[DEBUG] Loading image from: " << imgData.filepath << std::endl;
        
        try {
            // Check if file exists and is readable
            if (!fs::exists(imgData.filepath)) {
                std::cerr << "[ERROR] File does not exist: " << imgData.filepath << std::endl;
                return;
            }
            
            // Check file size
            auto fileSize = fs::file_size(imgData.filepath);
            std::cout << "[DEBUG] File size: " << fileSize << " bytes" << std::endl;
            
            if (fileSize == 0) {
                std::cerr << "[ERROR] File is empty: " << imgData.filepath << std::endl;
                return;
            }
            
            if (fileSize > 50 * 1024 * 1024) { // 50MB limit
                std::cerr << "[WARNING] File is very large: " << fileSize << " bytes" << std::endl;
            }
              imgData.image = cv::imread(imgData.filepath, cv::IMREAD_COLOR);
            
            if (imgData.image.empty()) {
                std::cerr << "[ERROR] Cannot load image: " << imgData.filepath << std::endl;
                // Try different loading modes
                std::cout << "[DEBUG] Trying to load with IMREAD_UNCHANGED flag" << std::endl;
                imgData.image = cv::imread(imgData.filepath, cv::IMREAD_UNCHANGED);
                
                if (imgData.image.empty()) {
                    std::cerr << "[ERROR] Failed to load image with any method" << std::endl;
                    return;
                }
            }
            
            std::cout << "[DEBUG] Image loaded successfully: " << imgData.image.cols << "x" << imgData.image.rows 
                      << ", channels: " << imgData.image.channels() << ", type: " << imgData.image.type() << std::endl;
            
            // Additional validation for the loaded image
            if (imgData.image.cols <= 0 || imgData.image.rows <= 0) {
                std::cerr << "[ERROR] Invalid image dimensions after loading: " << imgData.image.cols << "x" << imgData.image.rows << std::endl;
                imgData.image.release();
                return;
            }
            
            // Check for reasonable image size limits
            if (imgData.image.cols > 16384 || imgData.image.rows > 16384) {
                std::cerr << "[ERROR] Image dimensions too large: " << imgData.image.cols << "x" << imgData.image.rows << std::endl;
                imgData.image.release();
                return;
            }
            
            // Validate image channels
            if (imgData.image.channels() < 1 || imgData.image.channels() > 4) {
                std::cerr << "[ERROR] Invalid number of channels: " << imgData.image.channels() << std::endl;
                imgData.image.release();
                return;
            }
            
            // Check image data type
            if (imgData.image.depth() != CV_8U) {
                std::cout << "[DEBUG] Converting image to 8-bit unsigned" << std::endl;
                cv::Mat converted;
                imgData.image.convertTo(converted, CV_8U);
                imgData.image = converted;
            }
            
            // Validate image data
            if (imgData.image.data == nullptr) {
                std::cerr << "[ERROR] Image data is null" << std::endl;
                imgData.image.release();
                return;
            }
            
            imgData.textureID = CreateTexture(imgData.image);
            if (imgData.textureID != 0) {
                imgData.textureLoaded = true;
                std::cout << "[DEBUG] Texture loaded successfully with ID: " << imgData.textureID << std::endl;
            } else {
                std::cerr << "[ERROR] Failed to create texture for: " << imgData.filename << std::endl;
                imgData.image.release(); // Free memory if texture creation failed
            }
        } catch (const cv::Exception& e) {
            std::cerr << "[ERROR] OpenCV exception loading image: " << e.what() << std::endl;
            imgData.image.release();
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Standard exception loading image: " << e.what() << std::endl;
            imgData.image.release();
        } catch (...) {
            std::cerr << "[ERROR] Unknown exception loading image: " << imgData.filepath << std::endl;
            imgData.image.release();
        }
    }

    // Load thumbnail texture
    void LoadThumbnailTexture(ImageData& imgData) {
        std::cout << "[DEBUG] LoadThumbnailTexture called for: " << imgData.filename << std::endl;
        
        if (imgData.thumbnailLoaded) {
            std::cout << "[DEBUG] Thumbnail already loaded, skipping" << std::endl;
            return;
        }
        
        try {
            if (imgData.image.empty()) {
                std::cout << "[DEBUG] Main image not loaded, loading for thumbnail" << std::endl;
                
                if (!fs::exists(imgData.filepath)) {
                    std::cerr << "[ERROR] File does not exist for thumbnail: " << imgData.filepath << std::endl;
                    return;
                }
                
                imgData.image = cv::imread(imgData.filepath, cv::IMREAD_COLOR);
                if (imgData.image.empty()) {
                    std::cerr << "[ERROR] Cannot load image for thumbnail: " << imgData.filepath << std::endl;
                    return;
                }
                std::cout << "[DEBUG] Image loaded for thumbnail: " << imgData.image.cols << "x" << imgData.image.rows << std::endl;
            }
            
            imgData.thumbnail = CreateThumbnail(imgData.image);
            if (imgData.thumbnail.empty()) {
                std::cerr << "[ERROR] Failed to create thumbnail for: " << imgData.filename << std::endl;
                return;
            }
            
            imgData.thumbnailTextureID = CreateTexture(imgData.thumbnail);
            if (imgData.thumbnailTextureID != 0) {
                imgData.thumbnailLoaded = true;
                std::cout << "[DEBUG] Thumbnail texture created successfully with ID: " << imgData.thumbnailTextureID << std::endl;
            } else {
                std::cerr << "[ERROR] Failed to create thumbnail texture for: " << imgData.filename << std::endl;
                imgData.thumbnail.release();
            }
        } catch (const cv::Exception& e) {
            std::cerr << "[ERROR] OpenCV exception creating thumbnail: " << e.what() << std::endl;
            imgData.thumbnail.release();
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Standard exception creating thumbnail: " << e.what() << std::endl;
            imgData.thumbnail.release();
        } catch (...) {
            std::cerr << "[ERROR] Unknown exception creating thumbnail for: " << imgData.filename << std::endl;
            imgData.thumbnail.release();
        }
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
        LoadImageList();        std::cout << "Initialization complete!" << std::endl;
        return true;
    }
      void Run() {
        std::cout << "Starting main loop..." << std::endl;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            // Check for ESC key to exit
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

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
                    
                    try {
                        if (ImGui::ImageButton(buttonId, reinterpret_cast<void*>(img.thumbnailTextureID), 
                                             ImVec2(thumbnailSize, thumbnailSize))) {
                            selectedImageIndex = i;
                            std::cout << "[DEBUG] Selected image: " << img.filename << std::endl;
                        }
                        
                        // Highlight selection
                        if (selectedImageIndex == i) {
                            ImVec2 rectMin = ImGui::GetItemRectMin();
                            ImVec2 rectMax = ImGui::GetItemRectMax();
                            ImGui::GetWindowDrawList()->AddRect(rectMin, rectMax, 
                                                              IM_COL32(255, 255, 0, 255), 0.0f, 0, 3.0f);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[ERROR] Exception displaying thumbnail for " << img.filename << ": " << e.what() << std::endl;
                        // Display error placeholder
                        ImGui::Button("ERROR", ImVec2(thumbnailSize, thumbnailSize));
                    } catch (...) {
                        std::cerr << "[ERROR] Unknown exception displaying thumbnail for " << img.filename << std::endl;
                        // Display error placeholder
                        ImGui::Button("ERROR", ImVec2(thumbnailSize, thumbnailSize));
                    }
                } else {
                    // Display loading placeholder
                    ImGui::Button("Loading...", ImVec2(thumbnailSize, thumbnailSize));
                    if (ImGui::IsItemClicked()) {
                        selectedImageIndex = i;
                        std::cout << "[DEBUG] Selected loading image: " << img.filename << std::endl;
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
                std::cout << "[DEBUG] Displaying selected image: " << selectedImg.filename << std::endl;
                
                // Load full image
                if (!selectedImg.textureLoaded) {
                    std::cout << "[DEBUG] Loading texture for selected image" << std::endl;
                    LoadImageTexture(selectedImg);
                }
                
                if (selectedImg.textureLoaded && !selectedImg.image.empty()) {
                    try {
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
                        
                        // Validate image size
                        if (imageSize.x <= 0 || imageSize.y <= 0) {
                            std::cerr << "[ERROR] Invalid display size: " << imageSize.x << "x" << imageSize.y << std::endl;
                            ImGui::Text("Error: Invalid image dimensions");
                        } else {
                            // Center display
                            ImVec2 cursorPos = ImGui::GetCursorPos();
                            ImGui::SetCursorPos(ImVec2(
                                cursorPos.x + (panelSize.x - imageSize.x) * 0.5f,
                                cursorPos.y + (panelSize.y - imageSize.y) * 0.5f
                            ));
                            
                            ImGui::Image(reinterpret_cast<void*>(selectedImg.textureID), imageSize);
                            std::cout << "[DEBUG] Image displayed successfully" << std::endl;
                        }
                        
                        // Display image info
                        ImGui::SetCursorPos(ImVec2(10, 10));
                        ImGui::Text("File: %s", selectedImg.filename.c_str());
                        ImGui::Text("Size: %dx%d", selectedImg.image.cols, selectedImg.image.rows);
                        ImGui::Text("Channels: %d", selectedImg.image.channels());
                        ImGui::Text("Texture ID: %u", selectedImg.textureID);
                        
                    } catch (const std::exception& e) {
                        std::cerr << "[ERROR] Exception displaying main image: " << e.what() << std::endl;
                        ImGui::Text("Error displaying image: %s", e.what());
                    } catch (...) {
                        std::cerr << "[ERROR] Unknown exception displaying main image" << std::endl;
                        ImGui::Text("Unknown error displaying image");
                    }
                } else if (!selectedImg.textureLoaded) {
                    ImGui::Text("Loading image...");
                    std::cout << "[DEBUG] Image still loading..." << std::endl;
                } else {
                    ImGui::Text("Failed to load image: %s", selectedImg.filename.c_str());
                    std::cerr << "[ERROR] Failed to load image: " << selectedImg.filename << std::endl;
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
    // Allocate console for Windows GUI application
    #ifdef _WIN32
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    #endif
    
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