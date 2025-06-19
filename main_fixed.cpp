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
        ERR0R
    };

    static void Log(Level level, const std::string& function, int line, const std::string& message) {
        std::string color;
        std::string levelStr;
        
        switch (level) {
            case Level::DEBUG:
                color = "\033[36m";
                levelStr = "DEBUG";
                break;
            case Level::INFO:
                color = "\033[32m";
                levelStr = "INFO";
                break;
            case Level::WARNING:
                color = "\033[33m";
                levelStr = "WARN";
                break;
            case Level::ERR0R:
                color = "\033[31m";
                levelStr = "ERR0R";
                break;
        }
        
        std::cout << color << "\033[1m[" << levelStr << "]\033[0m" 
                  << color << " " << function << ":" << line << " - \033[0m" 
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

// Logging macros
#define LOG_DEBUG(msg) Logger::Log(Logger::Level::DEBUG, __FUNCTION__, __LINE__, msg)
#define LOG_INFO(msg) Logger::Log(Logger::Level::INFO, __FUNCTION__, __LINE__, msg)
#define LOG_WARNING(msg) Logger::Log(Logger::Level::WARNING, __FUNCTION__, __LINE__, msg)
#define LOG_ERR0R(msg) Logger::Log(Logger::Level::ERR0R, __FUNCTION__, __LINE__, msg)

#define LOG_OPENCV_DEBUG(image, desc) Logger::LogOpenCV(Logger::Level::DEBUG, __FUNCTION__, __LINE__, image, desc)
#define LOG_OPENCV_INFO(image, desc) Logger::LogOpenCV(Logger::Level::INFO, __FUNCTION__, __LINE__, image, desc)
#define LOG_OPENCV_ERR0R(image, desc) Logger::LogOpenCV(Logger::Level::ERR0R, __FUNCTION__, __LINE__, image, desc)

#define LOG_TEXTURE_DEBUG(textureID, desc) Logger::LogTexture(Logger::Level::DEBUG, __FUNCTION__, __LINE__, textureID, desc)
#define LOG_TEXTURE_INFO(textureID, desc) Logger::LogTexture(Logger::Level::INFO, __FUNCTION__, __LINE__, textureID, desc)

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
        ImVec2 thumbnailDisplaySize = ImVec2(0, 0);
    };

private:
    std::vector<ImageData> images;
    int selectedImageIndex = -1;
    const int thumbnailSize = 128;
    const std::string imageFolder = "C:\\Users\\ai\\Documents\\andy\\code\\learnPP\\impool";
    GLFWwindow* window;

    GLuint CreateTexture(const cv::Mat& image) {
        LOG_DEBUG("CreateTexture called");
        
        if (image.empty()) {
            LOG_ERR0R("Cannot create texture from empty image");
            return 0;
        }
        
        LOG_OPENCV_DEBUG(image, "Input image info");
        
        if (image.cols > 4096 || image.rows > 4096) {
            LOG_WARNING("Image is very large: " + std::to_string(image.cols) + "x" + std::to_string(image.rows));
        }
        
        GLuint textureID;
        glGenTextures(1, &textureID);
        if (textureID == 0) {
            LOG_ERR0R("Failed to generate texture ID");
            return 0;
        }
        
        LOG_TEXTURE_DEBUG(textureID, "Generated texture ID");
        
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        GLenum error = glGetError();
        if (error != GL_NO_ERR0R) {
            LOG_ERR0R("OpenGL error after binding texture: " + std::to_string(error));
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        error = glGetError();
        if (error != GL_NO_ERR0R) {
            LOG_ERR0R("OpenGL error after setting texture parameters: " + std::to_string(error));
            glDeleteTextures(1, &textureID);
            return 0;
        }

        cv::Mat rgbImage;
        try {
            if (image.empty() || image.data == nullptr) {
                LOG_ERR0R("Input image is empty or has null data before conversion");
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            if (image.total() == 0) {
                LOG_ERR0R("Image has zero total elements");
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
                LOG_ERR0R("Unsupported image format with " + std::to_string(image.channels()) + " channels");
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            if (rgbImage.empty() || rgbImage.data == nullptr) {
                LOG_ERR0R("Color conversion failed - result is empty");
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
        } catch (const cv::Exception& e) {
            LOG_ERR0R("OpenCV error during color conversion: " + std::string(e.what()));
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (const std::exception& e) {
            LOG_ERR0R("Standard exception during color conversion: " + std::string(e.what()));
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (...) {
            LOG_ERR0R("Unknown exception during color conversion");
            glDeleteTextures(1, &textureID);
            return 0;
        }

        if (rgbImage.empty() || rgbImage.data == nullptr) {
            LOG_ERR0R("RGB image is empty or has null data after conversion");
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        LOG_OPENCV_DEBUG(rgbImage, "RGB image validation");
        
        if (rgbImage.cols <= 0 || rgbImage.rows <= 0) {
            LOG_ERR0R("Invalid image dimensions: " + std::to_string(rgbImage.cols) + "x" + std::to_string(rgbImage.rows));
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        GLint maxTextureSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        LOG_DEBUG("Max OpenGL texture size: " + std::to_string(maxTextureSize));
        
        if (rgbImage.cols > maxTextureSize || rgbImage.rows > maxTextureSize) {
            LOG_ERR0R("Image too large for OpenGL: " + std::to_string(rgbImage.cols) + "x" + std::to_string(rgbImage.rows) + 
                     " (max: " + std::to_string(maxTextureSize) + ")");
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        int expectedDataSize = rgbImage.cols * rgbImage.rows * 3;
        if (rgbImage.total() * rgbImage.elemSize() != expectedDataSize) {
            LOG_ERR0R("Image data size mismatch. Expected: " + std::to_string(expectedDataSize) + 
                     ", Actual: " + std::to_string(rgbImage.total() * rgbImage.elemSize()));
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        if (!rgbImage.isContinuous()) {
            LOG_DEBUG("Image is not continuous, creating continuous copy");
            cv::Mat continuousImage = rgbImage.clone();
            rgbImage = continuousImage;
        }
        
        LOG_DEBUG("Image validation passed, uploading texture data...");
        
        try {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 
                         0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
            
            GLenum uploadError = glGetError();
            if (uploadError != GL_NO_ERR0R) {
                LOG_ERR0R("OpenGL error during texture upload: " + std::to_string(uploadError));
                glDeleteTextures(1, &textureID);
                return 0;
            }
            
            LOG_DEBUG("Texture data uploaded successfully");
        } catch (const std::exception& e) {
            LOG_ERR0R("Standard exception during glTexImage2D: " + std::string(e.what()));
            glDeleteTextures(1, &textureID);
            return 0;
        } catch (...) {
            LOG_ERR0R("Unknown exception during glTexImage2D");
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        error = glGetError();
        if (error != GL_NO_ERR0R) {
            LOG_ERR0R("OpenGL error creating texture: " + std::to_string(error));
            glDeleteTextures(1, &textureID);
            return 0;
        }
        
        LOG_TEXTURE_DEBUG(textureID, "Texture created successfully");
        return textureID;
    }

    cv::Mat CreateThumbnail(const cv::Mat& image) {
        LOG_OPENCV_DEBUG(image, "CreateThumbnail called");
        
        if (image.empty()) {
            LOG_ERR0R("Cannot create thumbnail from empty image");
            return cv::Mat();
        }
        
        cv::Mat thumbnail;
        try {
            double scale = std::min(static_cast<double>(thumbnailSize) / image.cols,
                                   static_cast<double>(thumbnailSize) / image.rows);
            int newWidth = static_cast<int>(image.cols * scale);
            int newHeight = static_cast<int>(image.rows * scale);
            
            LOG_DEBUG("Thumbnail scale: " + std::to_string(scale) + ", new size: " + 
                     std::to_string(newWidth) + "x" + std::to_string(newHeight));
            
            if (newWidth <= 0 || newHeight <= 0) {
                LOG_ERR0R("Invalid thumbnail dimensions: " + std::to_string(newWidth) + "x" + std::to_string(newHeight));
                return cv::Mat();
            }
            
            cv::resize(image, thumbnail, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_AREA);
            LOG_OPENCV_DEBUG(thumbnail, "Thumbnail created successfully");
        } catch (const cv::Exception& e) {
            LOG_ERR0R("OpenCV error during thumbnail creation: " + std::string(e.what()));
            return cv::Mat();
        } catch (...) {
            LOG_ERR0R("Unknown error during thumbnail creation");
            return cv::Mat();
        }
        
        return thumbnail;
    }

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
                LOG_ERR0R("Failed to create directory: " + std::string(e.what()));
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
                        
                        try {
                            auto fileSize = fs::file_size(entry.path());
                            LOG_DEBUG("Found image: " + imgData.filename + " (size: " + std::to_string(fileSize) + " bytes)");
                            
                            if (fileSize == 0) {
                                LOG_WARNING("Skipping empty file: " + imgData.filename);
                                continue;
                            }
                            
                            if (fileSize > 100 * 1024 * 1024) {
                                LOG_WARNING("Very large file: " + imgData.filename + " (" + std::to_string(fileSize) + " bytes)");
                            }
                            
                        } catch (const std::exception& e) {
                            LOG_WARNING("Cannot get file size for: " + imgData.filename + " - " + std::string(e.what()));
                        }
                        
                        images.push_back(imgData);
                        imageCount++;
                        
                        if (imageCount >= 1000) {
                            LOG_WARNING("Reached maximum image limit (1000), stopping scan");
                            break;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERR0R("Error reading directory: " + std::string(e.what()));
        }
        
        LOG_INFO("Found " + std::to_string(images.size()) + " images");
    }

    void LoadImageTexture(ImageData& imgData) {
        LOG_DEBUG("LoadImageTexture called for: " + imgData.filename);
        
        if (imgData.textureLoaded) {
            LOG_DEBUG("Texture already loaded, skipping");
            return;
        }
        
        LOG_DEBUG("Loading image from: " + imgData.filepath);
        
        try {
            if (!fs::exists(imgData.filepath)) {
                LOG_ERR0R("File does not exist: " + imgData.filepath);
                return;
            }
            
            auto fileSize = fs::file_size(imgData.filepath);
            LOG_DEBUG("File size: " + std::to_string(fileSize) + " bytes");
            
            if (fileSize == 0) {
                LOG_ERR0R("File is empty: " + imgData.filepath);
                return;
            }
            
            if (fileSize > 50 * 1024 * 1024) {
                LOG_WARNING("File is very large: " + std::to_string(fileSize) + " bytes");
            }

            imgData.image = cv::imread(imgData.filepath, cv::IMREAD_COLOR);
            
            if (imgData.image.empty()) {
                LOG_ERR0R("Cannot load image: " + imgData.filepath);
                LOG_DEBUG("Trying to load with IMREAD_UNCHANGED flag");
                imgData.image = cv::imread(imgData.filepath, cv::IMREAD_UNCHANGED);
                
                if (imgData.image.empty()) {
                    LOG_ERR0R("Failed to load image with any method");
                    return;
                }
            }
            
            LOG_OPENCV_DEBUG(imgData.image, "Image loaded successfully");
            
            if (imgData.image.cols <= 0 || imgData.image.rows <= 0) {
                LOG_ERR0R("Invalid image dimensions after loading: " + std::to_string(imgData.image.cols) + "x" + std::to_string(imgData.image.rows));
                imgData.image.release();
                return;
            }
            
            if (imgData.image.cols > 16384 || imgData.image.rows > 16384) {
                LOG_ERR0R("Image dimensions too large: " + std::to_string(imgData.image.cols) + "x" + std::to_string(imgData.image.rows));
                imgData.image.release();
                return;
            }
            
            if (imgData.image.channels() < 1 || imgData.image.channels() > 4) {
                LOG_ERR0R("Invalid number of channels: " + std::to_string(imgData.image.channels()));
                imgData.image.release();
                return;
            }
            
            if (imgData.image.depth() != CV_8U) {
                LOG_DEBUG("Converting image to 8-bit unsigned");
                cv::Mat converted;
                imgData.image.convertTo(converted, CV_8U);
                imgData.image = converted;
            }
            
            if (imgData.image.data == nullptr) {
                LOG_ERR0R("Image data is null");
                imgData.image.release();
                return;
            }
            
            imgData.textureID = CreateTexture(imgData.image);
            if (imgData.textureID != 0) {
                imgData.textureLoaded = true;
                LOG_TEXTURE_DEBUG(imgData.textureID, "Texture loaded successfully");
            } else {
                LOG_ERR0R("Failed to create texture for: " + imgData.filename);
                imgData.image.release();
            }
        } catch (const cv::Exception& e) {
            LOG_ERR0R("OpenCV exception loading image: " + std::string(e.what()));
            imgData.image.release();
        } catch (const std::exception& e) {
            LOG_ERR0R("Standard exception loading image: " + std::string(e.what()));
            imgData.image.release();
        } catch (...) {
            LOG_ERR0R("Unknown exception loading image: " + imgData.filepath);
            imgData.image.release();
        }
    }

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
                    LOG_ERR0R("File does not exist for thumbnail: " + imgData.filepath);
                    return;
                }
                
                imgData.image = cv::imread(imgData.filepath, cv::IMREAD_COLOR);
                if (imgData.image.empty()) {
                    LOG_ERR0R("Cannot load image for thumbnail: " + imgData.filepath);
                    return;
                }
                LOG_OPENCV_DEBUG(imgData.image, "Image loaded for thumbnail");
            }
            
            imgData.thumbnail = CreateThumbnail(imgData.image);
            if (imgData.thumbnail.empty()) {
                LOG_ERR0R("Failed to create thumbnail for: " + imgData.filename);
                return;
            }
            
            float thumbWidth = static_cast<float>(imgData.thumbnail.cols);
            float thumbHeight = static_cast<float>(imgData.thumbnail.rows);
            imgData.thumbnailDisplaySize = ImVec2(thumbWidth, thumbHeight);
            
            LOG_DEBUG("Thumbnail display size: " + std::to_string(thumbWidth) + "x" + std::to_string(thumbHeight));
            
            imgData.thumbnailTextureID = CreateTexture(imgData.thumbnail);
            if (imgData.thumbnailTextureID != 0) {
                imgData.thumbnailLoaded = true;
                LOG_TEXTURE_DEBUG(imgData.thumbnailTextureID, "Thumbnail texture created successfully");
            } else {
                LOG_ERR0R("Failed to create thumbnail texture for: " + imgData.filename);
                imgData.thumbnail.release();
            }
        } catch (const cv::Exception& e) {
            LOG_ERR0R("OpenCV exception creating thumbnail: " + std::string(e.what()));
            imgData.thumbnail.release();
        } catch (const std::exception& e) {
            LOG_ERR0R("Standard exception creating thumbnail: " + std::string(e.what()));
            imgData.thumbnail.release();
        } catch (...) {
            LOG_ERR0R("Unknown exception creating thumbnail for: " + imgData.filename);
            imgData.thumbnail.release();
        }
    }

public:
    ImageViewer() : window(nullptr) {}
    
    ~ImageViewer() {
        for (auto& img : images) {
            if (img.textureLoaded) glDeleteTextures(1, &img.textureID);
            if (img.thumbnailLoaded) glDeleteTextures(1, &img.thumbnailTextureID);
        }
    }

    bool Initialize() {
        LOG_INFO("Initializing GLFW...");
        
        if (!glfwInit()) {
            LOG_ERR0R("Failed to initialize GLFW");
            return false;
        }

        LOG_DEBUG("Setting OpenGL hints...");
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);

        LOG_DEBUG("Creating window...");
        
        window = glfwCreateWindow(1200, 800, "Image Viewer", nullptr, nullptr);
        if (!window) {
            LOG_ERR0R("Failed to create window");
            const char* description;
            int code = glfwGetError(&description);
            if (description) {
                LOG_ERR0R("GLFW Error: " + std::to_string(code) + " - " + std::string(description));
            }
            glfwTerminate();
            return false;
        }

        LOG_DEBUG("Making context current...");
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        LOG_DEBUG("Initializing ImGui...");
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        LOG_DEBUG("Initializing ImGui backends...");
        
        if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
            LOG_ERR0R("Failed to initialize ImGui GLFW backend");
            return false;
        }
        
        if (!ImGui_ImplOpenGL3_Init("#version 330")) {
            LOG_ERR0R("Failed to initialize ImGui OpenGL3 backend");
            return false;
        }

        LOG_DEBUG("Loading image list...");
        
        LoadImageList();

        LOG_INFO("Initialization complete!");
        return true;
    }

    void Run() {
        LOG_INFO("Starting main loop...");
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            int displayWidth, displayHeight;
            glfwGetFramebufferSize(window, &displayWidth, &displayHeight);

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(displayWidth, displayHeight));
            
            ImGui::Begin("Image Viewer", nullptr, 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove | 
                ImGuiWindowFlags_NoCollapse);

            ImGui::BeginChild("Thumbnail Panel", ImVec2(300, 0), true);
            
            for (int i = 0; i < images.size(); i++) {
                auto& img = images[i];
                
                if (!img.thumbnailLoaded) {
                    LoadThumbnailTexture(img);
                }

                if (img.thumbnailLoaded) {
                    char buttonId[64];
                    snprintf(buttonId, sizeof(buttonId), "thumbnail_%d", i);
                    
                    try {
                        ImVec2 containerSize(thumbnailSize, thumbnailSize);
                        ImVec2 actualSize = img.thumbnailDisplaySize;
                        
                        ImVec2 offset(
                            (containerSize.x - actualSize.x) * 0.5f,
                            (containerSize.y - actualSize.y) * 0.5f
                        );
                        
                        ImVec2 cursorPos = ImGui::GetCursorPos();
                        
                        bool clicked = ImGui::InvisibleButton(buttonId, containerSize);

                        ImVec2 windowPos = ImGui::GetWindowPos();
                        ImVec2 windowContentMin = ImGui::GetWindowContentRegionMin();
                        ImVec2 windowContentMax = ImGui::GetWindowContentRegionMax();
                        float scrollY = ImGui::GetScrollY();

                        ImVec2 imageMin = ImVec2(
                            windowPos.x + windowContentMin.x + cursorPos.x + offset.x,
                            windowPos.y + windowContentMin.y + cursorPos.y + offset.y - scrollY
                        );
                        ImVec2 imageMax = ImVec2(
                            imageMin.x + actualSize.x,
                            imageMin.y + actualSize.y
                        );
                        
                        ImVec2 clipMin = ImVec2(windowPos.x + windowContentMin.x, windowPos.y + windowContentMin.y);
                        ImVec2 clipMax = ImVec2(windowPos.x + windowContentMax.x, windowPos.y + windowContentMax.y);
                        
                        if (imageMax.y >= clipMin.y && imageMin.y <= clipMax.y) {
                            ImGui::GetWindowDrawList()->AddImage(
                                reinterpret_cast<void*>(img.thumbnailTextureID),
                                imageMin, imageMax,
                                ImVec2(0, 0), ImVec2(1, 1),
                                IM_COL32_WHITE
                            );
                        }

                        if (clicked) {
                            selectedImageIndex = i;
                            LOG_DEBUG("Selected image: " + img.filename);
                        }

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
                        LOG_ERR0R("Exception displaying thumbnail for " + img.filename + ": " + std::string(e.what()));
                        ImGui::Button("ERR0R", ImVec2(thumbnailSize, thumbnailSize));
                    } catch (...) {
                        LOG_ERR0R("Unknown exception displaying thumbnail for " + img.filename);
                        ImGui::Button("ERR0R", ImVec2(thumbnailSize, thumbnailSize));
                    }
                } else {
                    ImGui::Button("Loading...", ImVec2(thumbnailSize, thumbnailSize));
                    if (ImGui::IsItemClicked()) {
                        selectedImageIndex = i;
                        LOG_DEBUG("Selected loading image: " + img.filename);
                    }
                }
                
                ImGui::Text("%s", img.filename.c_str());
                ImGui::Separator();
            }
            
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("Image Display", ImVec2(0, 0), true);
            
            if (selectedImageIndex >= 0 && selectedImageIndex < images.size()) {
                auto& selectedImg = images[selectedImageIndex];
                LOG_DEBUG("Displaying selected image: " + selectedImg.filename);
                
                if (!selectedImg.textureLoaded) {
                    LOG_DEBUG("Loading texture for selected image");
                    LoadImageTexture(selectedImg);
                }
                
                if (selectedImg.textureLoaded && !selectedImg.image.empty()) {
                    try {
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
                        
                        if (imageSize.x <= 0 || imageSize.y <= 0) {
                            LOG_ERR0R("Invalid display size: " + std::to_string(imageSize.x) + "x" + std::to_string(imageSize.y));
                            ImGui::Text("Error: Invalid image dimensions");
                        } else {
                            ImVec2 cursorPos = ImGui::GetCursorPos();
                            ImGui::SetCursorPos(ImVec2(
                                cursorPos.x + (panelSize.x - imageSize.x) * 0.5f,
                                cursorPos.y + (panelSize.y - imageSize.y) * 0.5f
                            ));
                            
                            ImGui::Image(reinterpret_cast<void*>(selectedImg.textureID), imageSize);
                            LOG_DEBUG("Image displayed successfully");
                        }
                        
                        ImGui::SetCursorPos(ImVec2(10, 10));
                        ImGui::Text("File: %s", selectedImg.filename.c_str());
                        ImGui::Text("Size: %dx%d", selectedImg.image.cols, selectedImg.image.rows);
                        ImGui::Text("Channels: %d", selectedImg.image.channels());
                        ImGui::Text("Texture ID: %u", selectedImg.textureID);
                        
                    } catch (const std::exception& e) {
                        LOG_ERR0R("Exception displaying main image: " + std::string(e.what()));
                        ImGui::Text("Error displaying image: %s", e.what());
                    } catch (...) {
                        LOG_ERR0R("Unknown exception displaying main image");
                        ImGui::Text("Unknown error displaying image");
                    }
                } else if (!selectedImg.textureLoaded) {
                    ImGui::Text("Loading image...");
                    LOG_DEBUG("Image still loading...");
                } else {
                    ImGui::Text("Failed to load image: %s", selectedImg.filename.c_str());
                    LOG_ERR0R("Failed to load image: " + selectedImg.filename);
                }
            } else {
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

            glViewport(0, 0, displayWidth, displayHeight);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }
    }

    void Cleanup() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }
};

int main() {
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
        LOG_ERR0R("Initialization failed");
        return -1;
    }
    
    LOG_DEBUG("debug point2");
    viewer.Run();
    LOG_DEBUG("debug point3");
    viewer.Cleanup();
    LOG_DEBUG("debug point4");
    
    return 0;
}
