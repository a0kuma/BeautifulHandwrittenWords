/**
 * !important! how to compile and run
 * rm build -r -fo; mkdir build; cd build; cmake .. -G "Visual Studio 17 2022" -A x64 ; cmake --build . --config Release; .\Release\ImageViewer.exe; cd ..
 */

#include <iostream>//do not remove
#include <vector>//do not remove
#include <string>//do not remove
#include <filesystem>//do not remove
#include <algorithm>//do not remove
#include <memory>//do not remove
#include <cstdio>//do not remove
#include <sstream>//do not remove
#include <iomanip>//do not remove

#ifdef _WIN32
#include <windows.h>//do not remove
#include <io.h>//do not remove
#include <fcntl.h>//do not remove
#endif

#include <opencv2/opencv.hpp>//do not remove
#include <GL/glew.h>//do not remove - must be included before other OpenGL headers
#include <imgui.h>//do not remove
#include <imgui_impl_glfw.h>//do not remove
#include <imgui_impl_opengl3.h>//do not remove
#include <GLFW/glfw3.h>//do not remove

using namespace std;//do not remove

// Simple helper function to load an image into a OpenGL texture using OpenCV
bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height)
{
    // Load image using OpenCV
    cv::Mat image = cv::imread(filename, cv::IMREAD_COLOR);
    if (image.empty())
        return false;

    // Convert BGR to RGB (OpenCV uses BGR by default)
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
    
    // Flip the image vertically (OpenGL expects origin at bottom-left, OpenCV at top-left)
    cv::flip(image, image, 0);

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.cols, image.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, image.data);

    *out_texture = image_texture;
    *out_width = image.cols;
    *out_height = image.rows;

    return true;
}

// Function to load and process image with OpenCV effects
bool LoadProcessedTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height, 
                                 float brightness = 0.0f, float contrast = 1.0f, int blur_kernel = 0, bool grayscale = false)
{
    // Load image using OpenCV
    cv::Mat image = cv::imread(filename, cv::IMREAD_COLOR);
    if (image.empty())
        return false;

    // Apply grayscale conversion if requested
    if (grayscale) {
        cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR); // Convert back to 3-channel for consistency
    }

    // Apply brightness and contrast adjustments
    if (brightness != 0.0f || contrast != 1.0f) {
        image.convertTo(image, -1, contrast, brightness);
    }

    // Apply blur if requested
    if (blur_kernel > 0) {
        cv::GaussianBlur(image, image, cv::Size(blur_kernel * 2 + 1, blur_kernel * 2 + 1), 0);
    }

    // Convert BGR to RGB (OpenCV uses BGR by default)
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
    
    // Flip the image vertically (OpenGL expects origin at bottom-left, OpenCV at top-left)
    cv::flip(image, image, 0);

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.cols, image.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, image.data);

    *out_texture = image_texture;
    *out_width = image.cols;
    *out_height = image.rows;

    return true;
}

int main()
{
    // Initialize GLFW
    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Hello World ImGui", NULL, NULL);
    if (window == NULL) {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        cerr << "Failed to initialize GLEW" << endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    bool show_directory_window = true;
    bool show_opencv_window = true;

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);    // Load image texture variables
    GLuint image_texture = 0;
    int image_width = 0;
    int image_height = 0;
    string current_image_path = "";
    
    // Image processing parameters
    static float brightness = 0.0f;
    static float contrast = 1.0f;
    static int blur_kernel = 0;
    static bool grayscale = false;
    
    // Function to load image using OpenCV
    auto load_image = [&](const string& path) {
        // Clean up previous texture
        if (image_texture != 0) {
            glDeleteTextures(1, &image_texture);
            image_texture = 0;
        }
        
        if (LoadTextureFromFile(path.c_str(), &image_texture, &image_width, &image_height)) {
            current_image_path = path;
            cout << "Successfully loaded image: " << path << " (" << image_width << "x" << image_height << ")" << endl;
        } else {
            cerr << "Error: Could not load image: " << path << endl;
            current_image_path = "";
        }
    };
    
    // Function to reload image with processing effects
    auto reload_with_effects = [&]() {
        if (!current_image_path.empty()) {
            if (image_texture != 0) {
                glDeleteTextures(1, &image_texture);
                image_texture = 0;
            }
            
            if (LoadProcessedTextureFromFile(current_image_path.c_str(), &image_texture, &image_width, &image_height,
                                           brightness, contrast, blur_kernel, grayscale)) {
                cout << "Reloaded image with effects applied" << endl;
            } else {
                cerr << "Error: Could not reload image with effects" << endl;
            }
        }
    };
    
    // Load initial image
    string initial_image_path = "C:/Users/ai/Documents/andy/code/learnPP/impool/IMGname (184).jpg";
    load_image(initial_image_path);

    // Directory listing state
    vector<string> directory_entries;
    string current_path = "../impool";
    
    // Function to refresh directory listing
    auto refresh_directory = [&]() {
        directory_entries.clear();
        try {
            for (const auto& entry : filesystem::directory_iterator(current_path)) {
                string name = entry.path().filename().string();
                if (entry.is_directory()) {
                    name += "/";
                }
                directory_entries.push_back(name);
            }
            sort(directory_entries.begin(), directory_entries.end());
        } catch (const filesystem::filesystem_error& ex) {
            directory_entries.clear();
            directory_entries.push_back("Error reading directory: " + string(ex.what()));
        }
    };    // Initial directory load
    refresh_directory();

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);
            ImGui::Checkbox("Directory Window", &show_directory_window);
            ImGui::Checkbox("OpenCV Window", &show_opencv_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }        // 4. Show directory listing window
        if (show_directory_window)
        {
            ImGui::Begin("Image Browser", &show_directory_window);
            
            ImGui::Text("Current Directory: %s", filesystem::absolute(current_path).string().c_str());
            ImGui::Text("Current Image: %s", current_image_path.empty() ? "None" : filesystem::path(current_image_path).filename().string().c_str());
            
            if (ImGui::Button("Refresh"))
                refresh_directory();
            
            ImGui::Separator();
            
            // Show directory contents with click to load functionality
            if (ImGui::BeginChild("DirectoryContents", ImVec2(0, 300), true))
            {
                for (const auto& entry : directory_entries)
                {
                    // Check if it's an image file
                    string ext = filesystem::path(entry).extension().string();
                    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    bool is_image = (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".tiff" || ext == ".tga");
                    
                    if (is_image) {
                        // Make image files clickable
                        if (ImGui::Selectable(entry.c_str())) {
                            string full_path = filesystem::absolute(current_path + "/" + entry).string();
                            load_image(full_path);
                        }
                        // Highlight current selected image
                        if (!current_image_path.empty() && filesystem::path(current_image_path).filename().string() == entry) {
                            ImGui::SameLine();
                            ImGui::Text("  <-- Current");
                        }
                    } else {
                        // Non-image files shown as regular text
                        ImGui::Text("%s", entry.c_str());
                    }
                }
            }
            ImGui::EndChild();
            
            ImGui::Text("Total items: %zu", directory_entries.size());
            ImGui::Text("Tip: Click on image files to load them");
            
            ImGui::End();
        }        // 5. Show OpenCV image window
        if (show_opencv_window)
        {
            ImGui::Begin("OpenCV Image Viewer", &show_opencv_window);
            
            if (!current_image_path.empty()) {
                ImGui::Text("File: %s", filesystem::path(current_image_path).filename().string().c_str());
                ImGui::Text("Full Path: %s", current_image_path.c_str());
            } else {
                ImGui::Text("No image loaded");
            }
            
            ImGui::Separator();
            
            // Display the image
            if (image_texture != 0)
            {
                ImGui::Text("Original size preview:");
                ImGui::Image((void*)(intptr_t)image_texture, ImVec2(128, 128));
                
                ImGui::Separator();
                
                // Show different scaled versions
                static float scale = 1.0f;
                ImGui::SliderFloat("Scale", &scale, 0.1f, 3.0f);
                
                float display_width = image_width * scale;
                float display_height = image_height * scale;
                
                // Limit display size to reasonable bounds
                float max_display = 800.0f;
                if (display_width > max_display || display_height > max_display) {
                    float ratio = min(max_display / display_width, max_display / display_height);
                    display_width *= ratio;
                    display_height *= ratio;
                }
                
                ImGui::Text("Scaled version (%.1fx):", scale);
                ImGui::Image((void*)(intptr_t)image_texture, ImVec2(display_width, display_height));
            }
            else
            {
                ImGui::Text("No image texture loaded");
                ImGui::Text("Select an image from the Image Browser");
            }
            
            ImGui::Separator();
            ImGui::Text("Image Properties:");
            ImGui::Text("  Dimensions: %dx%d pixels", image_width, image_height);
            ImGui::Text("  Format: RGB (3 channels)");
            if (image_width > 0 && image_height > 0) {
                float aspect_ratio = (float)image_width / (float)image_height;
                ImGui::Text("  Aspect Ratio: %.3f", aspect_ratio);
                ImGui::Text("  Total Pixels: %d", image_width * image_height);
            }
            
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }    // Cleanup
    if (image_texture != 0)
        glDeleteTextures(1, &image_texture);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}