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
#include <sstream>
#include <iomanip>

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

// Centralized Logging System
class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

private:
    static const std::string RESET;
    static const std::string RED;
    static const std::string GREEN;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string MAGENTA;
    static const std::string CYAN;
    static const std::string WHITE;
    static const std::string BOLD;

public:
    static void Log(Level level, const std::string& function, int line, const std::string& message) {
        std::string color;
        std::string levelStr;
        
        switch (level) {
            case Level::DEBUG:
                color = CYAN;
                levelStr = "DEBUG";
                break;
            case Level::INFO:
                color = GREEN;
                levelStr = "INFO";
                break;
            case Level::WARNING:
                color = YELLOW;
                levelStr = "WARN";
                break;
            case Level::ERROR:
                color = RED;
                levelStr = "ERROR";
                break;
        }
        
        std::cout << color << BOLD << "[" << levelStr << "]" << RESET 
                  << color << " " << function << ":" << line << " - " << RESET 
                  << message << std::endl;
    }
    
    static void LogOpenCV(Level level, const std::string& function, int line, const cv::Mat& image, const std::string& desc = "") {
        std::stringstream ss;
        if (!desc.empty()) ss << desc << " - ";
        ss << "Image[" << image.cols << "x" << image.rows 
           << ", channels:" << image.channels() 
           << ", type:" << image.type() << "]";
        Log(level, function, line, ss.str());
    }
    
    static void LogTexture(Level level, const std::string& function, int line, GLuint textureID, const std::string& desc = "") {
        std::stringstream ss;
        if (!desc.empty()) ss << desc << " - ";
        ss << "Texture ID: " << textureID;
        Log(level, function, line, ss.str());
    }
};

// Color constants
const std::string Logger::RESET = "\033[0m";
const std::string Logger::RED = "\033[31m";
const std::string Logger::GREEN = "\033[32m";
const std::string Logger::YELLOW = "\033[33m";
const std::string Logger::BLUE = "\033[34m";
const std::string Logger::MAGENTA = "\033[35m";
const std::string Logger::CYAN = "\033[36m";
const std::string Logger::WHITE = "\033[37m";
const std::string Logger::BOLD = "\033[1m";

// Logging macros
#define LOG_DEBUG(msg) Logger::Log(Logger::Level::DEBUG, __FUNCTION__, __LINE__, msg)
#define LOG_INFO(msg) Logger::Log(Logger::Level::INFO, __FUNCTION__, __LINE__, msg)
#define LOG_WARNING(msg) Logger::Log(Logger::Level::WARNING, __FUNCTION__, __LINE__, msg)
#define LOG_ERROR(msg) Logger::Log(Logger::Level::ERROR, __FUNCTION__, __LINE__, msg)

#define LOG_OPENCV_DEBUG(image, desc) Logger::LogOpenCV(Logger::Level::DEBUG, __FUNCTION__, __LINE__, image, desc)
#define LOG_OPENCV_INFO(image, desc) Logger::LogOpenCV(Logger::Level::INFO, __FUNCTION__, __LINE__, image, desc)
#define LOG_OPENCV_ERROR(image, desc) Logger::LogOpenCV(Logger::Level::ERROR, __FUNCTION__, __LINE__, image, desc)

#define LOG_TEXTURE_DEBUG(textureID, desc) Logger::LogTexture(Logger::Level::DEBUG, __FUNCTION__, __LINE__, textureID, desc)
#define LOG_TEXTURE_INFO(textureID, desc) Logger::LogTexture(Logger::Level::INFO, __FUNCTION__, __LINE__, textureID, desc)

class ImageViewer {
public:    struct ImageData {
        std::string filename;
        std::string filepath;
        cv::Mat image;
        cv::Mat thumbnail;
        GLuint textureID = 0;
        GLuint thumbnailTextureID = 0;
        bool textureLoaded = false;
        bool thumbnailLoaded = false;
        ImVec2 thumbnailDisplaySize = ImVec2(0, 0); // Actual display size for thumbnail
    };

private:
    std::vector<ImageData> images;
    int selectedImageIndex = -1;
    const int thumbnailSize = 128;
    const std::string imageFolder = "C:\\Users\\ai\\Documents\\andy\\code\\learnPP\\impool";
    GLFWwindow* window;    // OpenGL texture creation helper
    GLuint CreateTexture(const cv::Mat& image) {
        LOG_DEBUG("CreateTexture called");
        
        if (image.empty()) {
            LOG_ERROR("Cannot create texture from empty image");
            return 0;
        }
        
        LOG_OPENCV_DEBUG(image, "Input image info");
        
        // Check if image is too large
        if (image.cols > 4096 || image.rows > 4096) {
            LOG_WARNING("Image is very large: " + std::to_string(image.cols) + "x" + std::to_string(image.rows));
        }
        
        GLuint textureID;
        glGenTextures(1, &textureID);
        if (textureID == 0) {
            LOG_ERROR("Failed to generate texture ID");
            return 0;
        }
        
        LOG_TEXTURE_DEBUG(textureID, "Generated texture ID");
        
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // Check for OpenGL errors after binding
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            LOG_ERROR("OpenGL error after binding texture: " + std::to_string(error));
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
            LOG_ERROR("OpenGL error after setting texture parameters: " + std::to_string(error));
            glDeleteTextures(1, &textureID);
            return 0;
        }          // Convert BGR to RGB
        cv::Mat rgbImage;
        try {
            // Additional validation before color conversion
            if (image.empty() || image.data == nullptr) {
                LOG_ERROR("Input image is empty or has null data before conversion");
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            // Check image data integrity
            if (image.total() == 0) {
                LOG_ERROR("Image has zero total elements");
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            LOG_OPENCV_DEBUG(image, "Converting image");
            
            if (image.channels() == 3) {
                cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
                LOG_DEBUG("Converted from BGR to RGB");
            } else if (image.channels() == 4) {
                cv::cvtColor(image, rgbImage, cv::COLOR_BGRA2RGB);
                LOG_DEBUG("Converted from BGRA to RGB");
            } else if (image.channels() == 1) {
                cv::cvtColor(image, rgbImage, cv::COLOR_GRAY2RGB);
                LOG_DEBUG("Converted from GRAY to RGB");
            } else {
                LOG_ERROR("Unsupported image format with " + std::to_string(image.channels()) + " channels");
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            // Validate conversion result
            if (rgbImage.empty() || rgbImage.data == nullptr) {
                LOG_ERROR("Color conversion failed - result is empty");
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
        } catch (const cv::Exception& e) {
            LOG_ERROR("OpenCV error during color conversion: " + std::string(e.what()));
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (const std::exception& e) {
            LOG_ERROR("Standard exception during color conversion: " + std::string(e.what()));
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (...) {
            LOG_ERROR("Unknown exception during color conversion");
            glDeleteTextures(1, &textureID);
            return 0;
        }          // Validate converted image
        if (rgbImage.empty() || rgbImage.data == nullptr) {
            LOG_ERROR("RGB image is empty or has null data after conversion");
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        LOG_OPENCV_DEBUG(rgbImage, "RGB image validation");
        
        // Additional validation checks
        if (rgbImage.cols <= 0 || rgbImage.rows <= 0) {
            LOG_ERROR("Invalid image dimensions: " + std::to_string(rgbImage.cols) + "x" + std::to_string(rgbImage.rows));
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Check if image is too large for OpenGL
        GLint maxTextureSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        LOG_DEBUG("Max OpenGL texture size: " + std::to_string(maxTextureSize));
        
        if (rgbImage.cols > maxTextureSize || rgbImage.rows > maxTextureSize) {
            LOG_ERROR("Image too large for OpenGL: " + std::to_string(rgbImage.cols) + "x" + std::to_string(rgbImage.rows) + 
                     " (max: " + std::to_string(maxTextureSize) + ")");
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Validate image data integrity
        int expectedDataSize = rgbImage.cols * rgbImage.rows * 3; // 3 channels for RGB
        if (rgbImage.total() * rgbImage.elemSize() != expectedDataSize) {
            LOG_ERROR("Image data size mismatch. Expected: " + std::to_string(expectedDataSize) + 
                     ", Actual: " + std::to_string(rgbImage.total() * rgbImage.elemSize()));
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Check if image data is continuous in memory
        if (!rgbImage.isContinuous()) {
            LOG_DEBUG("Image is not continuous, creating continuous copy");
            cv::Mat continuousImage = rgbImage.clone();
            rgbImage = continuousImage;
        }
        
        LOG_DEBUG("Image validation passed, uploading texture data...");
        
        // Upload texture data
        try {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 
                         0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
            
            // Check for OpenGL errors immediately after texture upload
            GLenum uploadError = glGetError();
            if (uploadError != GL_NO_ERROR) {
                LOG_ERROR("OpenGL error during texture upload: " + std::to_string(uploadError));
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            LOG_DEBUG("Texture data uploaded successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("Standard exception during glTexImage2D: " + std::string(e.what()));
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (...) {
            LOG_ERROR("Unknown exception during glTexImage2D");
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        // Check for OpenGL errors
        error = glGetError();
        if (error != GL_NO_ERROR) {
            LOG_ERROR("OpenGL error creating texture: " + std::to_string(error));
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        LOG_TEXTURE_DEBUG(textureID, "Texture created successfully");
        return textureID;
    }    // Create thumbnail with proper aspect ratio
    cv::Mat CreateThumbnail(const cv::Mat& image) {
        LOG_OPENCV_DEBUG(image, "CreateThumbnail called");
        
        if (image.empty()) {
            LOG_ERROR("Cannot create thumbnail from empty image");
            return cv::Mat();
        }
        
        cv::Mat thumbnail;
        try {
            // Calculate scale to fit within thumbnail size while maintaining aspect ratio
            double scale = std::min(static_cast<double>(thumbnailSize) / image.cols,
                                   static_cast<double>(thumbnailSize) / image.rows);
            int newWidth = static_cast<int>(image.cols * scale);
            int newHeight = static_cast<int>(image.rows * scale);
            
            LOG_DEBUG("Thumbnail scale: " + std::to_string(scale) + ", new size: " + 
                     std::to_string(newWidth) + "x" + std::to_string(newHeight));
            
            if (newWidth <= 0 || newHeight <= 0) {
                LOG_ERROR("Invalid thumbnail dimensions: " + std::to_string(newWidth) + "x" + std::to_string(newHeight));
                return cv::Mat();
            }
            
            // Resize with proper interpolation
            cv::resize(image, thumbnail, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_AREA);
            LOG_OPENCV_DEBUG(thumbnail, "Thumbnail created successfully");
        } catch (const cv::Exception& e) {
            LOG_ERROR("OpenCV error during thumbnail creation: " + std::string(e.what()));
            return cv::Mat();
        } catch (...) {
            LOG_ERROR("Unknown error during thumbnail creation");
            return cv::Mat();
        }
        
        return thumbnail;
    }// Load all images
    void LoadImageList() {
        LOG_DEBUG("LoadImageList called");
        images.clear();
        
        LOG_INFO("Checking image folder: " + imageFolder);
        
        if (!fs::exists(imageFolder)) {
            LOG_WARNING("Folder does not exist: " + imageFolder);
            LOG_INFO("Creating the folder...");
            try {
                fs::create_directories(imageFolder);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to create directory: " + std::string(e.what()));
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
                            LOG_DEBUG("Found image: " + imgData.filename + " (size: " + std::to_string(fileSize) + " bytes)");
                            
                            if (fileSize == 0) {
                                LOG_WARNING("Skipping empty file: " + imgData.filename);
                                continue;
                            }
                            
                            if (fileSize > 100 * 1024 * 1024) { // 100MB limit
                                LOG_WARNING("Very large file: " + imgData.filename + " (" + std::to_string(fileSize) + " bytes)");
                            }
                            
                        } catch (const std::exception& e) {
                            LOG_WARNING("Cannot get file size for: " + imgData.filename + " - " + std::string(e.what()));
                        }
                        
                        images.push_back(imgData);
                        imageCount++;
                        
                        // Limit number of images to prevent memory issues
                        if (imageCount >= 1000) {
                            LOG_WARNING("Reached maximum image limit (1000), stopping scan");
                            break;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error reading directory: " + std::string(e.what()));
        }
        
        LOG_INFO("Found " + std::to_string(images.size()) + " images");
    }    // Load image texture
    void LoadImageTexture(ImageData& imgData) {
        LOG_DEBUG("LoadImageTexture called for: " + imgData.filename);
        
        if (imgData.textureLoaded) {
            LOG_DEBUG("Texture already loaded, skipping");
            return;
        }
        
        LOG_DEBUG("Loading image from: " + imgData.filepath);
        
        try {
            // Check if file exists and is readable
            if (!fs::exists(imgData.filepath)) {
                LOG_ERROR("File does not exist: " + imgData.filepath);
                return;
            }
            
            // Check file size
            auto fileSize = fs::file_size(imgData.filepath);
            LOG_DEBUG("File size: " + std::to_string(fileSize) + " bytes");
            
            if (fileSize == 0) {
                LOG_ERROR("File is empty: " + imgData.filepath);
                return;
            }
            
            if (fileSize > 50 * 1024 * 1024) { // 50MB limit
                LOG_WARNING("File is very large: " + std::to_string(fileSize) + " bytes");
            }
              imgData.image = cv::imread(imgData.filepath, cv::IMREAD_COLOR);
            
            if (imgData.image.empty()) {
                LOG_ERROR("Cannot load image: " + imgData.filepath);
                // Try different loading modes
                LOG_DEBUG("Trying to load with IMREAD_UNCHANGED flag");
                imgData.image = cv::imread(imgData.filepath, cv::IMREAD_UNCHANGED);
                
                if (imgData.image.empty()) {
                    LOG_ERROR("Failed to load image with any method");
                    return;
                }
            }
            
            LOG_OPENCV_DEBUG(imgData.image, "Image loaded successfully");
            
            // Additional validation for the loaded image
            if (imgData.image.cols <= 0 || imgData.image.rows <= 0) {
                LOG_ERROR("Invalid image dimensions after loading: " + std::to_string(imgData.image.cols) + "x" + std::to_string(imgData.image.rows));
                imgData.image.release();
                return;
            }
            
            // Check for reasonable image size limits
            if (imgData.image.cols > 16384 || imgData.image.rows > 16384) {
                LOG_ERROR("Image dimensions too large: " + std::to_string(imgData.image.cols) + "x" + std::to_string(imgData.image.rows));
                imgData.image.release();
                return;
            }
            
            // Validate image channels
            if (imgData.image.channels() < 1 || imgData.image.channels() > 4) {
                LOG_ERROR("Invalid number of channels: " + std::to_string(imgData.image.channels()));
                imgData.image.release();
                return;
            }
            
            // Check image data type
            if (imgData.image.depth() != CV_8U) {
                LOG_DEBUG("Converting image to 8-bit unsigned");
                cv::Mat converted;
                imgData.image.convertTo(converted, CV_8U);
                imgData.image = converted;
            }
            
            // Validate image data
            if (imgData.image.data == nullptr) {
                LOG_ERROR("Image data is null");
                imgData.image.release();
                return;
            }
            
            imgData.textureID = CreateTexture(imgData.image);
            if (imgData.textureID != 0) {
                imgData.textureLoaded = true;
                LOG_TEXTURE_DEBUG(imgData.textureID, "Texture loaded successfully");
            } else {
                LOG_ERROR("Failed to create texture for: " + imgData.filename);
                imgData.image.release(); // Free memory if texture creation failed
            }
        } catch (const cv::Exception& e) {
            LOG_ERROR("OpenCV exception loading image: " + std::string(e.what()));
            imgData.image.release();
        } catch (const std::exception& e) {
            LOG_ERROR("Standard exception loading image: " + std::string(e.what()));
            imgData.image.release();
        } catch (...) {
            LOG_ERROR("Unknown exception loading image: " + imgData.filepath);
            imgData.image.release();
        }
    }    // Load thumbnail texture
    void LoadThumbnailTexture(ImageData& imgData) {
        LOG_DEBUG("LoadThumbnailTexture called for: " + imgData.filename);
        
        if (imgData.thumbnailLoaded) {
            LOG_DEBUG("Thumbnail already loaded, skipping");
            return;
        }
        
        try {
            if (imgData.image.empty()) {
                LOG_DEBUG("Main image not loaded, loading for thumbnail");
                
                if (!fs::exists(imgData.filepath)) {
                    LOG_ERROR("File does not exist for thumbnail: " + imgData.filepath);
                    return;
                }
                
                imgData.image = cv::imread(imgData.filepath, cv::IMREAD_COLOR);
                if (imgData.image.empty()) {
                    LOG_ERROR("Cannot load image for thumbnail: " + imgData.filepath);
                    return;
                }
                LOG_OPENCV_DEBUG(imgData.image, "Image loaded for thumbnail");
            }
            
            imgData.thumbnail = CreateThumbnail(imgData.image);
            if (imgData.thumbnail.empty()) {
                LOG_ERROR("Failed to create thumbnail for: " + imgData.filename);
                return;
            }
            
            // Calculate display size for thumbnail to maintain aspect ratio within thumbnail area
            float thumbWidth = static_cast<float>(imgData.thumbnail.cols);
            float thumbHeight = static_cast<float>(imgData.thumbnail.rows);
            imgData.thumbnailDisplaySize = ImVec2(thumbWidth, thumbHeight);
            
            LOG_DEBUG("Thumbnail display size: " + std::to_string(thumbWidth) + "x" + std::to_string(thumbHeight));
            
            imgData.thumbnailTextureID = CreateTexture(imgData.thumbnail);
            if (imgData.thumbnailTextureID != 0) {
                imgData.thumbnailLoaded = true;
                LOG_TEXTURE_DEBUG(imgData.thumbnailTextureID, "Thumbnail texture created successfully");
            } else {
                LOG_ERROR("Failed to create thumbnail texture for: " + imgData.filename);
                imgData.thumbnail.release();
            }
        } catch (const cv::Exception& e) {
            LOG_ERROR("OpenCV exception creating thumbnail: " + std::string(e.what()));
            imgData.thumbnail.release();
        } catch (const std::exception& e) {
            LOG_ERROR("Standard exception creating thumbnail: " + std::string(e.what()));
            imgData.thumbnail.release();
        } catch (...) {            LOG_ERROR("Unknown exception creating thumbnail for: " + imgData.filename);
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
        LOG_INFO("Initializing GLFW...");
        
        // Initialize GLFW
        if (!glfwInit()) {
            LOG_ERROR("Failed to initialize GLFW");
            return false;
        }

        LOG_DEBUG("Setting OpenGL hints...");
        
        // Set OpenGL version - try more compatible settings
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);

        LOG_DEBUG("Creating window...");
        
        // Create windowed mode first (easier to debug)
        window = glfwCreateWindow(1200, 800, "Image Viewer", nullptr, nullptr);
        if (!window) {
            LOG_ERROR("Failed to create window");
            const char* description;
            int code = glfwGetError(&description);
            if (description) {
                LOG_ERROR("GLFW Error: " + std::to_string(code) + " - " + std::string(description));
            }
            glfwTerminate();
            return false;
        }

        LOG_DEBUG("Making context current...");
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Vertical sync

        LOG_DEBUG("Initializing ImGui...");
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Set ImGui style
        ImGui::StyleColorsDark();

        LOG_DEBUG("Initializing ImGui backends...");
        
        // Initialize ImGui backends
        if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
            LOG_ERROR("Failed to initialize ImGui GLFW backend");
            return false;
        }
        
        if (!ImGui_ImplOpenGL3_Init("#version 330")) {
            LOG_ERROR("Failed to initialize ImGui OpenGL3 backend");
            return false;
        }

        LOG_DEBUG("Loading image list...");
        
        // Load image list
        LoadImageList();        LOG_INFO("Initialization complete!");
        return true;
    }      void Run() {
        LOG_INFO("Starting main loop...");
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
                }if (img.thumbnailLoaded) {
                    // Display thumbnail with proper aspect ratio
                    char buttonId[64];
                    snprintf(buttonId, sizeof(buttonId), "thumbnail_%d", i);
                    
                    try {
                        // Create a container for the thumbnail with fixed size
                        ImVec2 containerSize(thumbnailSize, thumbnailSize);
                        ImVec2 actualSize = img.thumbnailDisplaySize;
                        
                        // Center the thumbnail within the container
                        ImVec2 offset(
                            (containerSize.x - actualSize.x) * 0.5f,
                            (containerSize.y - actualSize.y) * 0.5f
                        );
                        
                        // Save cursor position
                        ImVec2 cursorPos = ImGui::GetCursorPos();
                        
                        // Create invisible button for the full container area
                        bool clicked = ImGui::InvisibleButton(buttonId, containerSize);                        // Draw the thumbnail image centered in the container
                        // Get the current window position and scroll offset
                        ImVec2 windowPos = ImGui::GetWindowPos();
                        ImVec2 windowContentMin = ImGui::GetWindowContentRegionMin();
                        ImVec2 windowContentMax = ImGui::GetWindowContentRegionMax();
                        float scrollY = ImGui::GetScrollY();                        // Calculate image position accounting for scroll
                        ImVec2 imageMin = ImVec2(
                            windowPos.x + windowContentMin.x + cursorPos.x + offset.x,
                            windowPos.y + windowContentMin.y + cursorPos.y + offset.y - scrollY
                        );
                        ImVec2 imageMax = ImVec2(
                            imageMin.x + actualSize.x,
                            imageMin.y + actualSize.y
                        );
                        
                        // Check if the image is visible in the current scroll area
                        ImVec2 clipMin = ImVec2(windowPos.x + windowContentMin.x, windowPos.y + windowContentMin.y);
                        ImVec2 clipMax = ImVec2(windowPos.x + windowContentMax.x, windowPos.y + windowContentMax.y);
                        
                        // Only draw if the image intersects with the visible area
                        if (imageMax.y >= clipMin.y && imageMin.y <= clipMax.y) {
                            // Draw the image
                            ImGui::GetWindowDrawList()->AddImage(
                                reinterpret_cast<void*>(img.thumbnailTextureID),
                                imageMin, imageMax,
                                ImVec2(0, 0), ImVec2(1, 1),  // UV coordinates
                                IM_COL32_WHITE  // Tint color
                            );
                        }
                          // Handle click
                        if (clicked) {
                            selectedImageIndex = i;
                            LOG_DEBUG("Selected image: " + img.filename);
                        }// Highlight selection with border around the container
                        if (selectedImageIndex == i) {
                            ImVec2 rectMin = ImVec2(
                                windowPos.x + windowContentMin.x + cursorPos.x,
                                windowPos.y + windowContentMin.y + cursorPos.y - scrollY
                            );
                            ImVec2 rectMax = ImVec2(rectMin.x + containerSize.x, rectMin.y + containerSize.y);
                            ImGui::GetWindowDrawList()->AddRect(rectMin, rectMax, 
                                                              IM_COL32(255, 255, 0, 255), 0.0f, 0, 3.0f);
                        }
                        
                    } catch (const std::exception& e) {
                        LOG_ERROR("Exception displaying thumbnail for " + img.filename + ": " + std::string(e.what()));                    // Display error placeholder
                        ImGui::Button("ERROR", ImVec2(thumbnailSize, thumbnailSize));
                    } catch (...) {
                        LOG_ERROR("Unknown exception displaying thumbnail for " + img.filename);
                        // Display error placeholder
                        ImGui::Button("ERROR", ImVec2(thumbnailSize, thumbnailSize));
                    }
                } else {
                    // Display loading placeholder
                    ImGui::Button("Loading...", ImVec2(thumbnailSize, thumbnailSize));
                    if (ImGui::IsItemClicked()) {
                        selectedImageIndex = i;
                        LOG_DEBUG("Selected loading image: " + img.filename);
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
                LOG_DEBUG("Displaying selected image: " + selectedImg.filename);
                
                // Load full image
                if (!selectedImg.textureLoaded) {
                    LOG_DEBUG("Loading texture for selected image");
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
                            LOG_ERROR("Invalid display size: " + std::to_string(imageSize.x) + "x" + std::to_string(imageSize.y));
                            ImGui::Text("Error: Invalid image dimensions");
                        } else {
                            // Center display
                            ImVec2 cursorPos = ImGui::GetCursorPos();
                            ImGui::SetCursorPos(ImVec2(
                                cursorPos.x + (panelSize.x - imageSize.x) * 0.5f,
                                cursorPos.y + (panelSize.y - imageSize.y) * 0.5f
                            ));
                            
                            ImGui::Image(reinterpret_cast<void*>(selectedImg.textureID), imageSize);
                            LOG_DEBUG("Image displayed successfully");
                        }
                        
                        // Display image info
                        ImGui::SetCursorPos(ImVec2(10, 10));
                        ImGui::Text("File: %s", selectedImg.filename.c_str());
                        ImGui::Text("Size: %dx%d", selectedImg.image.cols, selectedImg.image.rows);
                        ImGui::Text("Channels: %d", selectedImg.image.channels());
                        ImGui::Text("Texture ID: %u", selectedImg.textureID);
                        
                    } catch (const std::exception& e) {                        LOG_ERROR("Exception displaying main image: " + std::string(e.what()));
                        ImGui::Text("Error displaying image: %s", e.what());
                    } catch (...) {
                        LOG_ERROR("Unknown exception displaying main image");
                        ImGui::Text("Unknown error displaying image");
                    }
                } else if (!selectedImg.textureLoaded) {
                    ImGui::Text("Loading image...");
                    LOG_DEBUG("Image still loading...");
                } else {
                    ImGui::Text("Failed to load image: %s", selectedImg.filename.c_str());
                    LOG_ERROR("Failed to load image: " + selectedImg.filename);
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
    
    LOG_INFO("Image Viewer Application Started");
    ImageViewer viewer;
    LOG_DEBUG("debug point1");
    
    if (!viewer.Initialize()) {
        LOG_ERROR("Initialization failed");
        return -1;
    }
    
    LOG_DEBUG("debug point2");
    viewer.Run();
    LOG_DEBUG("debug point3");
    viewer.Cleanup();
    LOG_DEBUG("debug point4");
    
    return 0;
}