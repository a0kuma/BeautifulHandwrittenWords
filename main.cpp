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
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;//do not remove

// Simple helper function to load an image into a OpenGL texture with common settings
bool LoadTextureFromMemory(const void* data, size_t data_size, GLuint* out_texture, int* out_width, int* out_height)
{
    // Load from file
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_texture = image_texture;
    *out_width = image_width;
    *out_height = image_height;

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
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    bool show_directory_window = true;
    bool show_opencv_window = true;    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Load image texture variables
    GLuint image_texture = 0;
    int image_width = 0;
    int image_height = 0;
    
    // Load image file into memory
    string image_path = "C:/Users/ai/Documents/andy/code/learnPP/impool/IMGname (184).jpg";
    FILE* file = fopen(image_path.c_str(), "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        vector<unsigned char> file_data(file_size);
        fread(file_data.data(), 1, file_size, file);
        fclose(file);
        
        // Load texture using the new function
        if (!LoadTextureFromMemory(file_data.data(), file_size, &image_texture, &image_width, &image_height)) {
            cerr << "Error: Could not load image texture!" << endl;
        }
    } else {
        cerr << "Error: Could not open image file!" << endl;
    }

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
            ImGui::Begin("Directory Listing", &show_directory_window);
            
            ImGui::Text("Current Directory: %s", filesystem::absolute(current_path).string().c_str());
            
            if (ImGui::Button("Refresh"))
                refresh_directory();
            
            ImGui::Separator();
            
            // Show directory contents
            if (ImGui::BeginChild("DirectoryContents", ImVec2(0, 300), true))
            {
                for (const auto& entry : directory_entries)
                {
                    ImGui::Text("%s", entry.c_str());
                }
            }
            ImGui::EndChild();
            
            ImGui::Text("Total items: %zu", directory_entries.size());
            
            ImGui::End();
        }

        // 5. Show OpenCV image window
        if (show_opencv_window)
        {
            ImGui::Begin("OpenCV Image", &show_opencv_window);
            
            ImGui::Text("OpenCV Image (48x48, RGB #ddb98b)");
            ImGui::Separator();
              // Display the image
            if (image_texture != 0)
            {
                ImGui::Image((void*)(intptr_t)image_texture, ImVec2(48, 48));
                
                // Also show a scaled up version for better visibility
                ImGui::Text("Scaled up version:");
                ImGui::Image((void*)(intptr_t)image_texture, ImVec2(192, 192));
            }
            else
            {
                ImGui::Text("Failed to load image texture");
            }
            
            ImGui::Text("Image size: %dx%d", image_width, image_height);
            ImGui::Text("Image channels: RGBA (4)");
            
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