/**
 * !important! how to compile and run
 * rm build -r -fo; mkdir build; cd build; cmake .. -G "Visual Studio 17 2022" -A x64 ; cmake --build . --config Release; .\Release\ImageViewer.exe; cd ..
 */

#include <iostream>   //do not remove
#include <vector>     //do not remove
#include <string>     //do not remove
#include <filesystem> //do not remove
#include <algorithm>  //do not remove
#include <memory>     //do not remove
#include <cstdio>     //do not remove
#include <sstream>    //do not remove
#include <iomanip>    //do not remove

#ifdef _WIN32
#include <windows.h> //do not remove
#include <io.h>      //do not remove
#include <fcntl.h>   //do not remove
#endif

#include <opencv2/opencv.hpp>   //do not remove
#include <GL/glew.h>            //do not remove - must be included before other OpenGL headers
#include <imgui.h>              //do not remove
#include <imgui_impl_glfw.h>    //do not remove
#include <imgui_impl_opengl3.h> //do not remove
#include <GLFW/glfw3.h>         //do not remove

#include <numeric>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <random>
#include <atomic>

using namespace std; // do not remove

/**
 * global variables for binary thresholding
 */
static int color_space = 1; // 0=RGB, 1=HSL, 2=HSV
static float rgb_threshold[3] = {128.0f, 128.0f, 128.0f};
static float hsl_threshold[3] = {0.0f, 0.0f, 68.0f};
static float hsv_threshold[3] = {180.0f, 50.0f, 50.0f};
cv::Mat image;

// 並查集（Disjoint‑Set Union）支援多執行緒
class ParallelDSU
{
public:
    ParallelDSU(size_t n) : parent(n), rank(n, 0), locks(n)
    {
        std::iota(parent.begin(), parent.end(), 0);
    }

    int find(int x)
    {
        while (true)
        {
            int p = parent[x].load(std::memory_order_acquire);
            if (p == x)
                return p;
            // path compression (無鎖)
            int gp = parent[p].load(std::memory_order_relaxed);
            if (gp == p)
                return gp;
            parent[x].compare_exchange_weak(p, gp, std::memory_order_release);
            x = gp;
        }
    }

    void unite(int a, int b)
    {
        while (true)
        {
            int ra = find(a);
            int rb = find(b);
            if (ra == rb)
                return;

            // 鎖定 root，避免同時 union 造成資料競爭
            std::scoped_lock lock(locks[std::min(ra, rb)], locks[std::max(ra, rb)]);
            // roots 可能在鎖前被改變，重新取得
            ra = find(ra);
            rb = find(rb);
            if (ra == rb)
                return;

            // union by rank
            if (rank[ra] < rank[rb])
            {
                parent[ra].store(rb, std::memory_order_release);
            }
            else if (rank[ra] > rank[rb])
            {
                parent[rb].store(ra, std::memory_order_release);
            }
            else
            {
                parent[rb].store(ra, std::memory_order_release);
                ++rank[ra];
            }
            return;
        }
    }

private:
    std::vector<std::atomic<int>> parent;
    std::vector<int> rank;
    std::vector<std::mutex> locks;
};

class MultithreadCluster
{
public:
    typedef struct Point2d
    {
        double x;
        double y;
    } Point2D; // 二維點

    vector<Point2D> formCV(vector<cv::Point> &points)
    {
        vector<Point2D> result;
        result.reserve(points.size());
        for (const auto &p : points)
        {
            result.push_back({static_cast<double>(p.x), static_cast<double>(p.y)});
        }
        return result;
    } // Convert cv::Point to Point2D

    // 計算平方距離
    inline double sqDist(const Point2D &a, const Point2D &b)
    {
        double dx = a.x - b.x;
        double dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    // 多執行緒叢集函式
    std::vector<std::vector<int>> cluster(
        const std::vector<Point2D> &points,
        double radius,
        unsigned thread_cnt = std::thread::hardware_concurrency())
    {
        const size_t n = points.size();
        ParallelDSU dsu(n);
        const double radius_sq = radius * radius;

        // Monte Carlo：隨機順序處理索引
        std::vector<size_t> indices(n);
        std::iota(indices.begin(), indices.end(), 0);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(indices.begin(), indices.end(), gen);

        // 每個 thread 處理分段區間
        std::vector<std::thread> workers;
        auto work = [&](size_t start_idx, size_t end_idx)
        {
            for (size_t idx = start_idx; idx < end_idx; ++idx)
            {
                size_t i = indices[idx];
                for (size_t j = i + 1; j < n; ++j)
                {
                    if (sqDist(points[i], points[j]) <= radius_sq)
                    {
                        dsu.unite(static_cast<int>(i), static_cast<int>(j));
                    }
                }
            }
        };

        size_t chunk = (n + thread_cnt - 1) / thread_cnt;
        size_t begin = 0;
        for (unsigned t = 0; t < thread_cnt; ++t)
        {
            size_t end = std::min(begin + chunk, n);
            if (begin >= end)
                break;
            workers.emplace_back(work, begin, end);
            begin = end;
        }
        for (auto &th : workers)
            th.join();

        // 收集叢集
        std::unordered_map<int, std::vector<int>> groups;
        for (size_t i = 0; i < n; ++i)
        {
            int root = dsu.find(static_cast<int>(i));
            groups[root].push_back(static_cast<int>(i));
        }

        std::vector<std::vector<int>> clusters;
        clusters.reserve(groups.size());
        for (auto &[root, vec] : groups)
        {
            clusters.emplace_back(std::move(vec));
        }
        return clusters;
    }
};

// Function to load and process image with OpenCV effects
bool LoadProcessedTextureFromFile(const char *filename, GLuint *out_texture, int *out_width, int *out_height,
                                  float brightness = 0.0f, float contrast = 1.0f, int blur_kernel = 0, bool grayscale = false,
                                  bool enable_binary = false, int color_space = 0,
                                  float rgb_threshold[3] = nullptr, float hsl_threshold[3] = nullptr, float hsv_threshold[3] = nullptr)
{
    // Load image using OpenCV with IMREAD_COLOR to ensure 3 channels
    image = cv::imread(filename, cv::IMREAD_COLOR);
    if (image.empty())
    {
        cerr << "Failed to load image for processing: " << filename << endl;
        return false;
    }

    // Ensure image is 3-channel BGR
    if (image.channels() != 3)
    {
        if (image.channels() == 4)
        {
            cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
        }
        else if (image.channels() == 1)
        {
            cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
        }
    }

    // Apply binary threshold if enabled
    if (enable_binary)
    {
        cv::Mat binary_mask;

        if (color_space == 0 && rgb_threshold)
        { // RGB
            cv::Mat bgr_channels[3];
            cv::split(image, bgr_channels);

            cv::Mat b_mask, g_mask, r_mask;
            cv::threshold(bgr_channels[0], b_mask, rgb_threshold[2], 255, cv::THRESH_BINARY); // B channel
            cv::threshold(bgr_channels[1], g_mask, rgb_threshold[1], 255, cv::THRESH_BINARY); // G channel
            cv::threshold(bgr_channels[2], r_mask, rgb_threshold[0], 255, cv::THRESH_BINARY); // R channel

            cv::bitwise_and(b_mask, g_mask, binary_mask);
            cv::bitwise_and(binary_mask, r_mask, binary_mask);
        }
        else if (color_space == 1 && hsl_threshold)
        { // HSL
            cv::Mat hls_image;
            cv::cvtColor(image, hls_image, cv::COLOR_BGR2HLS);

            cv::Mat hls_channels[3];
            cv::split(hls_image, hls_channels);

            cv::Mat h_mask, l_mask, s_mask;
            // H channel (0-180 in OpenCV)
            cv::threshold(hls_channels[0], h_mask, hsl_threshold[0], 255, cv::THRESH_BINARY);
            // L channel (0-255)
            cv::threshold(hls_channels[1], l_mask, hsl_threshold[2] * 255.0f / 100.0f, 255, cv::THRESH_BINARY);
            // S channel (0-255)
            cv::threshold(hls_channels[2], s_mask, hsl_threshold[1] * 255.0f / 100.0f, 255, cv::THRESH_BINARY);

            cv::bitwise_and(h_mask, l_mask, binary_mask);
            cv::bitwise_and(binary_mask, s_mask, binary_mask);
        }
        else if (color_space == 2 && hsv_threshold)
        { // HSV
            cv::Mat hsv_image;
            cv::cvtColor(image, hsv_image, cv::COLOR_BGR2HSV);

            cv::Mat hsv_channels[3];
            cv::split(hsv_image, hsv_channels);

            cv::Mat h_mask, s_mask, v_mask;
            // H channel (0-180 in OpenCV)
            cv::threshold(hsv_channels[0], h_mask, hsv_threshold[0], 255, cv::THRESH_BINARY);
            // S channel (0-255)
            cv::threshold(hsv_channels[1], s_mask, hsv_threshold[1] * 255.0f / 100.0f, 255, cv::THRESH_BINARY);
            // V channel (0-255)
            cv::threshold(hsv_channels[2], v_mask, hsv_threshold[2] * 255.0f / 100.0f, 255, cv::THRESH_BINARY);

            cv::bitwise_and(h_mask, s_mask, binary_mask);
            cv::bitwise_and(binary_mask, v_mask, binary_mask);
        } // Apply binary threshold to create pure binary image (0 or 1 values)
        if (!binary_mask.empty())
        {
            // Convert binary mask to pure binary values (0 or 1)
            cv::Mat pure_binary;
            binary_mask.convertTo(pure_binary, CV_8UC1, 1.0 / 255.0); // Convert 255 to 1, 0 stays 0

            // Convert to 3-channel for consistency with the rest of the pipeline
            cv::Mat binary_3channel;
            cv::cvtColor(pure_binary, binary_3channel, cv::COLOR_GRAY2BGR);

            // Scale back to 0-255 range for display purposes
            binary_3channel.convertTo(image, CV_8UC3, 255.0); // Convert 1 to 255, 0 stays 0
        }
    }

    // Apply grayscale conversion if requested
    if (grayscale)
    {
        cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR); // Convert back to 3-channel for consistency
    }

    // Apply brightness and contrast adjustments
    if (brightness != 0.0f || contrast != 1.0f)
    {
        image.convertTo(image, -1, contrast, brightness);
    }

    // Apply blur if requested
    if (blur_kernel > 0)
    {
        cv::GaussianBlur(image, image, cv::Size(blur_kernel * 2 + 1, blur_kernel * 2 + 1), 0);
    }

    // Convert BGR to RGB (OpenCV uses BGR by default)
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

    // Ensure continuous memory layout
    if (!image.isContinuous())
    {
        image = image.clone();
    }

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload pixels into texture - specify correct alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    // Upload as RGB 3-channel
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.cols, image.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, image.data);

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        cerr << "OpenGL error after processed texture upload: " << error << endl;
        glDeleteTextures(1, &image_texture);
        return false;
    }

    *out_texture = image_texture;
    *out_width = image.cols;
    *out_height = image.rows;

    return true;
}

int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    GLFWwindow *window = glfwCreateWindow(1920, 1080, "Handwriting maintenance system", NULL, NULL);
    if (window == NULL)
    {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize GLEW
    if (glewInit() != GLEW_OK)
    {
        cerr << "Failed to initialize GLEW" << endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark(); // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    bool show_directory_window = true;
    bool show_opencv_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Load image texture variables
    GLuint image_texture = 0;
    int image_width = 0;
    int image_height = 0;
    string current_image_path = "";

    // Image processing parameters
    static float brightness = 0.0f;
    static float contrast = 1.0f;
    static int blur_kernel = 0;
    static bool grayscale = false;

    // Binary threshold parameters
    static bool enable_binary = false;

    auto load_image = [&](const string &path)
    {
        // Clean up previous texture
        if (image_texture != 0)
        {
            glDeleteTextures(1, &image_texture);
            image_texture = 0;
        }

        if (LoadProcessedTextureFromFile(path.c_str(), &image_texture, &image_width, &image_height,
                                         brightness, contrast, blur_kernel, grayscale,
                                         enable_binary, color_space, rgb_threshold, hsl_threshold, hsv_threshold))
        {
            current_image_path = path;
            cout << "Successfully loaded image: " << path << " (" << image_width << "x" << image_height << ")" << endl;
        }
        else
        {
            cerr << "Error: Could not load image: " << path << endl;
            current_image_path = "";
        }
    };
    // Function to reload image with processing effects
    auto reload_with_effects = [&]()
    {
        if (!current_image_path.empty())
        {
            if (image_texture != 0)
            {
                glDeleteTextures(1, &image_texture);
                image_texture = 0;
            }

            if (LoadProcessedTextureFromFile(current_image_path.c_str(), &image_texture, &image_width, &image_height,
                                             brightness, contrast, blur_kernel, grayscale,
                                             enable_binary, color_space, rgb_threshold, hsl_threshold, hsv_threshold))
            {
                cout << "Reloaded image with effects applied" << endl;
            }
            else
            {
                cerr << "Error: Could not reload image with effects" << endl;
            }
        }
    }; // Find and load initial image with *184* in filename
    string initial_image_path = "";
    string search_directory = "../impool";

    try
    {
        for (const auto &entry : filesystem::directory_iterator(search_directory))
        {
            if (entry.is_regular_file())
            {
                string filename = entry.path().filename().string();
                string ext = entry.path().extension().string();
                transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                bool is_image = (ext == ".webp" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".tiff" || ext == ".tga");

                if (is_image && filename.find("184") != string::npos)
                {
                    initial_image_path = filesystem::absolute(entry.path()).string();
                    cout << "Found initial image: " << initial_image_path << endl;
                    break;
                }
            }
        }
    }
    catch (const filesystem::filesystem_error &ex)
    {
        cerr << "Error searching for initial image: " << ex.what() << endl;
    }

    if (!initial_image_path.empty())
    {
        load_image(initial_image_path);
    }
    else
    {
        cout << "No image with '184' in filename found in " << search_directory << endl;
    }

    // Directory listing state
    vector<string> directory_entries;
    string current_path = "../impool";

    // Function to refresh directory listing
    auto refresh_directory = [&]()
    {
        directory_entries.clear();
        try
        {
            for (const auto &entry : filesystem::directory_iterator(current_path))
            {
                string name = entry.path().filename().string();
                if (entry.is_directory())
                {
                    name += "/";
                }
                directory_entries.push_back(name);
            }
            sort(directory_entries.begin(), directory_entries.end());
        }
        catch (const filesystem::filesystem_error &ex)
        {
            directory_entries.clear();
            directory_entries.push_back("Error reading directory: " + string(ex.what()));
        }
    }; // Initial directory load
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

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {

            ImGui::Begin("main window");
            ImGui::Checkbox("Directory Window", &show_directory_window);
            ImGui::Checkbox("OpenCV Window", &show_opencv_window);

            if (ImGui::Button("Compare non-zero points to total pixels"))
            {
                if (!image.empty())
                {
                    cv::Mat gray_image;
                    cv::cvtColor(image, gray_image, cv::COLOR_RGB2GRAY);

                    cv::bitwise_not(gray_image, gray_image);

                    vector<cv::Point> nonZeroPoints;
                    cv::findNonZero(gray_image, nonZeroPoints);

                    size_t non_zero_count = nonZeroPoints.size();
                    size_t total_pixels = gray_image.total(); // total() returns rows * cols

                    cout << "Comparison of non-zero points to total pixels:" << endl;
                    cout << " - Non-zero points found: " << non_zero_count << endl;
                    cout << " - Total pixels in image: " << total_pixels << endl;

                    if (non_zero_count == total_pixels)
                    {
                        cout << "Result: All pixels in the image are non-zero." << endl;
                    }
                    else
                    {
                        cout << "Result: Not all pixels in the image are non-zero." << endl;
                    }

/**
 * do clustering
 */
                    MultithreadCluster clusterer;
                    vector<MultithreadCluster::Point2D> points = clusterer.formCV(nonZeroPoints);
                    double radius = 5.0; // Example radius for clustering
                    auto clusters = clusterer.cluster(points, radius);

                // Display clusters as ImGui::Selectable
                ImGui::Begin("Clusters");
                for (size_t i = 0; i < clusters.size(); ++i)
                {
                    std::ostringstream oss;
                    oss << "Cluster " << i << " (" << clusters[i].size() << " points)";
                    ImGui::Selectable(oss.str().c_str());
                }
                ImGui::End();



                }
                else
                {
                    cout << "No image loaded to perform the check." << endl;
                }
            }

            ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 4. Show directory listing window
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
                for (const auto &entry : directory_entries)
                {
                    // Check if it's an image file
                    string ext = filesystem::path(entry).extension().string();
                    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    bool is_image = (ext == ".webp" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".tiff" || ext == ".tga");

                    if (is_image)
                    {
                        // Make image files clickable
                        if (ImGui::Selectable(entry.c_str()))
                        {
                            string full_path = filesystem::absolute(current_path + "/" + entry).string();
                            load_image(full_path);
                        }
                        // Highlight current selected image
                        if (!current_image_path.empty() && filesystem::path(current_image_path).filename().string() == entry)
                        {
                            ImGui::SameLine();
                            ImGui::Text("  <-- Current");
                        }
                    }
                    else
                    {
                        // Non-image files shown as regular text
                        ImGui::Text("%s", entry.c_str());
                    }
                }
            }
            ImGui::EndChild();
            ImGui::Text("Total items: %zu", directory_entries.size());
            ImGui::Text("Tip: Click on image files to load them");

            ImGui::End();
        }

        // 5. Show OpenCV image window
        if (show_opencv_window)
        {
            ImGui::Begin("OpenCV Image Viewer & Processor", &show_opencv_window);

            if (!current_image_path.empty())
            {
                ImGui::Text("File: %s", filesystem::path(current_image_path).filename().string().c_str());
                ImGui::Text("Full Path: %s", current_image_path.c_str());
            }
            else
            {
                ImGui::Text("No image loaded");
            }

            ImGui::Separator();
            // Image processing controls
            if (!current_image_path.empty())
            {
                ImGui::Text("Image Processing Controls:");

                bool effects_changed = false;

                if (ImGui::SliderFloat("Brightness", &brightness, -100.0f, 100.0f))
                {
                    effects_changed = true;
                }

                if (ImGui::SliderFloat("Contrast", &contrast, 0.1f, 3.0f))
                {
                    effects_changed = true;
                }

                if (ImGui::SliderInt("Blur", &blur_kernel, 0, 10))
                {
                    effects_changed = true;
                }

                if (ImGui::Checkbox("Grayscale", &grayscale))
                {
                    effects_changed = true;
                }

                ImGui::Separator();

                // Binary threshold controls
                if (ImGui::Checkbox("Enable Binary Threshold", &enable_binary))
                {
                    effects_changed = true;
                }

                if (enable_binary)
                {
                    ImGui::Text("Color Space Selection:");

                    const char *color_space_items[] = {"RGB", "HSL", "HSV"};
                    if (ImGui::Combo("Color Space", &color_space, color_space_items, IM_ARRAYSIZE(color_space_items)))
                    {
                        effects_changed = true;
                    }

                    ImGui::Text("Threshold Controls:");

                    if (color_space == 0)
                    { // RGB
                        ImGui::Text("RGB Thresholds (0-255):");
                        if (ImGui::SliderFloat("Red", &rgb_threshold[0], 0.0f, 255.0f))
                        {
                            effects_changed = true;
                        }
                        if (ImGui::SliderFloat("Green", &rgb_threshold[1], 0.0f, 255.0f))
                        {
                            effects_changed = true;
                        }
                        if (ImGui::SliderFloat("Blue", &rgb_threshold[2], 0.0f, 255.0f))
                        {
                            effects_changed = true;
                        }
                    }
                    else if (color_space == 1)
                    { // HSL
                        ImGui::Text("HSL Thresholds:");
                        if (ImGui::SliderFloat("Hue", &hsl_threshold[0], 0.0f, 180.0f))
                        {
                            effects_changed = true;
                        }
                        if (ImGui::SliderFloat("Saturation", &hsl_threshold[1], 0.0f, 100.0f))
                        {
                            effects_changed = true;
                        }
                        if (ImGui::SliderFloat("Lightness", &hsl_threshold[2], 0.0f, 100.0f))
                        {
                            effects_changed = true;
                        }
                    }
                    else if (color_space == 2)
                    { // HSV
                        ImGui::Text("HSV Thresholds:");
                        if (ImGui::SliderFloat("Hue", &hsv_threshold[0], 0.0f, 180.0f))
                        {
                            effects_changed = true;
                        }
                        if (ImGui::SliderFloat("Saturation", &hsv_threshold[1], 0.0f, 100.0f))
                        {
                            effects_changed = true;
                        }
                        if (ImGui::SliderFloat("Value", &hsv_threshold[2], 0.0f, 100.0f))
                        {
                            effects_changed = true;
                        }
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Reset Effects"))
                {
                    brightness = 0.0f;
                    contrast = 1.0f;
                    blur_kernel = 0;
                    grayscale = false;
                    enable_binary = false;
                    color_space = 0;
                    rgb_threshold[0] = rgb_threshold[1] = rgb_threshold[2] = 128.0f;
                    hsl_threshold[0] = hsv_threshold[0] = 180.0f;
                    hsl_threshold[1] = hsl_threshold[2] = 50.0f;
                    hsv_threshold[1] = hsv_threshold[2] = 50.0f;
                    effects_changed = true;
                }

                if (effects_changed)
                {
                    reload_with_effects();
                }

                ImGui::Separator();
            }

            // Display the image
            if (image_texture != 0)
            {
                ImGui::Text("Image Preview:");

                // Show different scaled versions
                static float scale = 0.5f;
                ImGui::SliderFloat("Display Scale", &scale, 0.1f, 2.0f);

                float display_width = image_width * scale;
                float display_height = image_height * scale;

                // Limit display size to reasonable bounds
                float max_display = 1920.0f;
                if (display_width > max_display || display_height > max_display)
                {
                    float ratio = min(max_display / display_width, max_display / display_height);
                    display_width *= ratio;
                    display_height *= ratio;
                }

                ImGui::Image((void *)(intptr_t)image_texture, ImVec2(display_width, display_height));
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
            if (image_width > 0 && image_height > 0)
            {
                float aspect_ratio = (float)image_width / (float)image_height;
                ImGui::Text("  Aspect Ratio: %.3f", aspect_ratio);
                ImGui::Text("  Total Pixels: %d", image_width * image_height);

                // Calculate file size estimation
                float size_mb = (image_width * image_height * 3) / (1024.0f * 1024.0f);
                ImGui::Text("  Memory Usage: %.2f MB (uncompressed)", size_mb);

                // Show file extension vs actual format warning
                if (!current_image_path.empty())
                {
                    string ext = filesystem::path(current_image_path).extension().string();
                    ImGui::Text("  File Extension: %s", ext.c_str());
                    if (ext == ".jpg" || ext == ".jpeg")
                    {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  Warning: Files may be WebP format with wrong extension!");
                    }
                }
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
    }

    // Cleanup
    if (image_texture != 0)
        glDeleteTextures(1, &image_texture);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}