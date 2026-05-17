// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "main.h"

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}



// Bible loading methods
std::vector<TestamentInfo> load_translation(const std::string& path)
{
    std::vector<TestamentInfo> result;
    std::ifstream file(path);
    if (!file.is_open()) return result;

    std::string line;
    int last_book_id = -1;
    int last_chapter = -1;
    BookInfo* book = nullptr;
    ChapterInfo* chapter = nullptr;

    while (std::getline(file, line))
    {
        if (line.empty()) continue;

        std::vector<std::string> fields;
        std::string f;
        bool in_q = false;
        for (size_t i = 0; i < line.size(); i++)
        {
            char c = line[i];
            if (c == '"')
            {
                if (in_q && i + 1 < line.size() && line[i + 1] == '"')
                {
                    f += '"'; i++;
                }
                else
                {
                    in_q = !in_q;
                }
            }
            else if (c == ',' && !in_q)
            {
                fields.push_back(f);
                f.clear();
            }
            else
            {
                f += c;
            }
        }
        fields.push_back(f);
        if (fields.size() < 6) continue;

        int t = std::stoi(fields[0]);       // testament
        int b = std::stoi(fields[1]);       // book id
        int c = std::stoi(fields[2]);       // chapter
        int v = std::stoi(fields[3]);       // verse
        std::string bn = fields[4];         // book name
        std::string txt = fields[5];        // verse text

        while ((int)result.size() < t)
        {
            int idx = (int)result.size() + 1;
            result.push_back({idx, idx == 1 ? "Old Testament" : "New Testament", {}});
        }

        // new book
        if (b != last_book_id)
        {
            result[t - 1].books.push_back({b, bn, {}});
            book = &result[t - 1].books.back();
            last_chapter = -1;
            last_book_id = b;
        }

        // new chapter
        if (c != last_chapter)
        {
            book->chapters.push_back({c, {}});
            chapter = &book->chapters.back();
            last_chapter = c;
        }
        chapter->verses.push_back({v, txt});
    }

    return result;
}




static std::map<std::string, std::vector<TestamentInfo>> s_trans_cache;

static const std::vector<TestamentInfo>& get_translation(const std::string& name)
{
    auto it = s_trans_cache.find(name);
    if (it == s_trans_cache.end())
    {
        auto data = load_translation("translations/done/" + name + ".csv");
        it = s_trans_cache.emplace(name, std::move(data)).first;
    }
    return it->second;
}

static void render_passage(const std::vector<TestamentInfo>& data, int book_id, int chapter, int verse_num)
{
    for (auto& t : data)
        for (auto& b : t.books)
            if (b.id == book_id)
            {
                for (auto& c : b.chapters)
                    if (c.num == chapter)
                    {
                        ImGui::Text("%s  -  Chapter %d", b.name.c_str(), chapter);
                        ImGui::Separator();
                        std::string passage;
                        for (auto& v : c.verses)
                            if (verse_num == -1 || v.num == verse_num)
                            {
                                char num_buf[16];
                                snprintf(num_buf, sizeof(num_buf), "%d. ", v.num);
                                passage += num_buf;
                                passage += v.text;
                                passage += "\n\n";
                            }
                        if (ImGui::Button("Copy"))
                            ImGui::SetClipboardText(passage.c_str());
                        ImGui::SameLine();
                        if (verse_num == -1)
                            ImGui::TextDisabled("%zu verses", c.verses.size());
                        else
                            ImGui::TextDisabled("Chapter %d : Verse %d", chapter, verse_num);
                        ImGui::BeginChild("##passage", ImVec2(-FLT_MIN, -FLT_MIN));
                        for (auto& v : c.verses)
                            if (verse_num == -1 || v.num == verse_num)
                                ImGui::TextWrapped("%d. %s", v.num, v.text.c_str());
                        ImGui::EndChild();
                        return;
                    }
                return;
            }
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "Tome Well", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.
#endif

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    // {
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);



    // Our state  !@!
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    static bool show_menu = true;
    static bool reset_layout = false;

    // Scan available translations
    static std::vector<std::string> g_translations;
    static bool g_translations_scanned = false;
    if (!g_translations_scanned)
    {
        g_translations_scanned = true;
        DIR* dir = opendir("translations/done");
        if (dir)
        {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                const char* name = entry->d_name;
                const char* dot = strrchr(name, '.');
                if (dot && strcmp(dot, ".csv") == 0)
                    g_translations.push_back(std::string(name, dot - name));
            }
            closedir(dir);
        }
        if (g_translations.empty())
            g_translations = {"acv", "asv", "louis_segond", "mkjv", "nasb", "net", "ylt"};
    }

    // Default translation for main window
    static std::string def_translat = "asv";

    // Extra windows
    struct ExtraWin { bool open = false; std::string translation = "asv"; };
    static ExtraWin translation_windows[16];

    // Navigation state (driven by treeview)
    static int nav_book = 1;
    static int nav_chapter = 1;
    static int nav_verse = -1;  // -1 means start from verse 1

    

    // Load custom state
    {
        FILE* f = fopen("tomewell_state.ini", "r");
        if (f)
        {
            char buf[256];
            if (fgets(buf, sizeof(buf), f))
            {
                buf[strcspn(buf, "\r\n")] = 0;
                if (strlen(buf) > 0) def_translat = buf;
            }
            for (int i = 0; i < 16; i++)
            {
                if (fgets(buf, sizeof(buf), f))
                {
                    int open = 0;
                    char trans[64] = "";
                    sscanf(buf, "%d %63s", &open, trans);
                    translation_windows[i].open = (open != 0);
                    translation_windows[i].translation = (strlen(trans) > 0) ? trans : "asv";
                }
            }
            fclose(f);
        }
    }


    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Key Handlers
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_Q))
        {
            glfwSetWindowShouldClose(window, true);
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_M))
        {
            show_menu = !show_menu;
        }

        // Create the main menu
        if (show_menu && ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Translation Window"))
                {
                    for (int i = 0; i < 16; i++)
                    {
                        if (!translation_windows[i].open) { translation_windows[i].open = true; break; }
                    }
                }
                if (ImGui::MenuItem("Quit"))
                {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::MenuItem("Reset Layout"))
                {
                    def_translat = "asv";
                    for (int i = 0; i < 16; i++)
                    {
                        translation_windows[i].open = false;
                        translation_windows[i].translation = "asv";
                    }
                    nav_book = 1;
                    nav_chapter = 1;
                    nav_verse = -1;
                    remove("tomewell_state.ini");
                    reset_layout = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Create the docking layout
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("MainDockSpace", nullptr, dockspace_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id);
        ImGui::End();

        // Reset layout if requested
        if (reset_layout)
        {
            reset_layout = false;
            remove("imgui.ini");
            ImGui::ClearIniSettings();
        }

        // Set up initial split
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
        if (node && !node->IsSplitNode())
        {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_left, dock_right;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, &dock_left, &dock_right);

            ImGui::DockBuilderDockWindow("Treeview", dock_left);
            ImGui::DockBuilderDockWindow("Main Translation", dock_right);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        // Show extra windows
        for (int i = 0; i < 16; i++)
        {
            if (!translation_windows[i].open) continue;
            char name[64];
            snprintf(name, sizeof(name), "Other Translation##%d", i);
            ImGui::Begin(name, &translation_windows[i].open);

            if (ImGui::BeginCombo("##trans", translation_windows[i].translation.c_str()))
            {
                for (auto& t : g_translations)
                {
                    bool sel = (t == translation_windows[i].translation);
                    if (ImGui::Selectable(t.c_str(), sel))
                        translation_windows[i].translation = t;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Window %d", i);

            const auto& data = get_translation(translation_windows[i].translation);
            render_passage(data, nav_book, nav_chapter, nav_verse);

            ImGui::End();
        }

        {
            ImGui::Begin("Main Translation");

            if (ImGui::BeginCombo("##trans", def_translat.c_str()))
            {
                for (auto& t : g_translations)
                {
                    bool sel = (t == def_translat);
                    if (ImGui::Selectable(t.c_str(), sel))
                        def_translat = t;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const auto& data = get_translation(def_translat);
            render_passage(data, nav_book, nav_chapter, nav_verse);

            ImGui::End();
        }
        {
            ImGui::Begin("Treeview");

            const auto& tree_data = get_translation(def_translat);
            for (auto& t : tree_data)
            {
                if (ImGui::TreeNodeEx(t.label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (auto& b : t.books)
                    {
                        ImGuiTreeNodeFlags b_flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (b.id == nav_book) b_flags |= ImGuiTreeNodeFlags_Selected;
                        bool b_open = ImGui::TreeNodeEx(b.name.c_str(), b_flags);
                        if (ImGui::IsItemClicked())
                        {
                            nav_book = b.id;
                            nav_chapter = 1;
                            nav_verse = -1;
                        }
                        if (b_open)
                        {
                            for (auto& c : b.chapters)
                            {
                                char ch_label[32];
                                snprintf(ch_label, sizeof(ch_label), "Chapter %d", c.num);
                                ImGuiTreeNodeFlags c_flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                                if (b.id == nav_book && c.num == nav_chapter)
                                    c_flags |= ImGuiTreeNodeFlags_Selected;
                                bool c_open = ImGui::TreeNodeEx(ch_label, c_flags);
                                if (ImGui::IsItemClicked())
                                {
                                    nav_book = b.id;
                                    nav_chapter = c.num;
                                    nav_verse = -1;
                                }
                                if (c_open)
                                {
                                    for (auto& v : c.verses)
                                    {
                                        char v_label[32];
                                        snprintf(v_label, sizeof(v_label), "%d", v.num);
                                        ImGui::PushID(v.num);
                                        ImGuiTreeNodeFlags v_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                                        ImGui::TreeNodeEx(v_label, v_flags);
                                        ImGui::PopID();
                                        if (ImGui::IsItemClicked())
                                        {
                                            nav_book = b.id;
                                            nav_chapter = c.num;
                                            nav_verse = v.num;
                                        }
                                    }
                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
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

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        // {
        //     GLFWwindow* backup_current_context = glfwGetCurrentContext();
        //     ImGui::UpdatePlatformWindows();
        //     ImGui::RenderPlatformWindowsDefault();
        //     glfwMakeContextCurrent(backup_current_context);
        // }

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();


    // Store custom state
    {
        FILE* f = fopen("tomewell_state.ini", "w");
        if (f)
        {
            fprintf(f, "%s\n", def_translat.c_str());
            for (int i = 0; i < 16; i++)
            {
                fprintf(f, "%d %s\n", translation_windows[i].open ? 1 : 0, translation_windows[i].translation.c_str());
            }
            fclose(f);
        }
    }

    return 0;
}
