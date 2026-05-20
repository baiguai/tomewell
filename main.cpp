// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "main.h"
#include "tinyfiledialogs.h"
#include <ctime>
#include <cstdlib>
#include <regex>

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

std::string exe_dir()
{
    static std::string cached;
    if (!cached.empty()) return cached;
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    wchar_t* sep = wcsrchr(buf, L'\\');
    if (sep) *sep = L'\0';
    char mb[MAX_PATH];
    wcstombs(mb, buf, MAX_PATH);
    cached = mb;
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1)
    {
        buf[len] = '\0';
        cached = dirname(buf);
    }
#endif
    return cached;
}

std::string timestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
    return buf;
}

std::vector<DataEntry> load_data_file(const std::string& path)
{
    std::vector<DataEntry> entries;
    std::ifstream f(path);
    if (!f.is_open()) return entries;
    try
    {
        auto j = nlohmann::json::parse(f);
        auto& arr = j["entries"];
        for (auto& item : arr)
        {
            DataEntry e;
            e.type = item.value("type", "note");
            e.book_id = item.value("book", 0);
            e.chapter = item.value("chapter", 0);
            e.verse = item.value("verse", -1);
            e.sel_start = item.value("sel_start", -1);
            e.sel_end = item.value("sel_end", -1);
            e.content = item.value("content", "");
            e.created = item.value("created", "");
            e.modified = item.value("modified", "");
            entries.push_back(e);
        }
    }
    catch (...) {}
    return entries;
}

void save_data_file(const std::string& path, const std::vector<DataEntry>& entries)
{
    nlohmann::json j;
    j["version"] = 1;
    auto& arr = j["entries"];
    for (auto& e : entries)
    {
        nlohmann::json item;
        item["type"] = e.type;
        item["book"] = e.book_id;
        item["chapter"] = e.chapter;
        item["verse"] = e.verse;
        item["sel_start"] = e.sel_start;
        item["sel_end"] = e.sel_end;
        item["content"] = e.content;
        item["created"] = e.created;
        item["modified"] = e.modified;
        arr.push_back(item);
    }
    std::ofstream f(path);
    if (f.is_open()) f << j.dump(2) << "\n";
}

void create_default_data_file(const std::string& path)
{
    std::ofstream f(path);
    if (f.is_open()) f << "{\n  \"version\": 1,\n  \"entries\": []\n}\n";
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
        if (!line.empty() && line.back() == '\r') line.pop_back();

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




static std::string g_highlight_query;
static std::string g_regex_error;
static bool g_tree_inited = false;
static bool g_scroll_to_verse = false;
static bool g_expand_all = false;
static bool g_collapse_all = false;
static std::vector<DataEntry> g_data_entries;
static std::string g_data_path;
static std::map<std::string, std::vector<TestamentInfo>> s_trans_cache;

static int find_entry(const std::vector<DataEntry>& entries, int book, int chapter, int verse);
static int find_bookmark(const std::vector<DataEntry>& entries, int book, int chapter, int verse);

struct NotesTreeVerse { int num; };
struct NotesTreeChapter { int num; bool has_chapter_note; std::vector<NotesTreeVerse> verses; };
struct NotesTreeBook { int id; std::string name; std::vector<NotesTreeChapter> chapters; };
struct NotesTreeTestament { int num; std::string label; std::vector<NotesTreeBook> books; };
static std::vector<NotesTreeTestament> g_notes_tree;
static bool g_notes_tree_dirty = true;

static void rebuild_notes_tree(const std::vector<TestamentInfo>& bible_data)
{
    g_notes_tree.clear();
    for (auto& t : bible_data)
    {
        NotesTreeTestament ntt;
        ntt.num = t.num;
        ntt.label = t.label;
        bool has_t = false;
        for (auto& b : t.books)
        {
            NotesTreeBook ntb;
            ntb.id = b.id;
            ntb.name = b.name;
            bool has_b = false;
            for (auto& c : b.chapters)
            {
                NotesTreeChapter ntc;
                ntc.num = c.num;
                ntc.has_chapter_note = (find_entry(g_data_entries, b.id, c.num, -1) >= 0);
                bool has_c = ntc.has_chapter_note;
                for (auto& v : c.verses)
                {
                    if (find_entry(g_data_entries, b.id, c.num, v.num) >= 0)
                    {
                        ntc.verses.push_back({v.num});
                        has_c = true;
                    }
                }
                if (has_c)
                {
                    ntb.chapters.push_back(ntc);
                    has_b = true;
                }
            }
            if (has_b)
            {
                ntt.books.push_back(ntb);
                has_t = true;
            }
        }
        if (has_t)
            g_notes_tree.push_back(ntt);
    }
    g_notes_tree_dirty = false;
}

static const std::vector<TestamentInfo>& get_translation(const std::string& name)
{
    auto it = s_trans_cache.find(name);
    if (it == s_trans_cache.end())
    {
        auto data = load_translation(exe_dir() + "/translations/" + name + ".csv");
        it = s_trans_cache.emplace(name, std::move(data)).first;
    }
    return it->second;
}

static void render_highlighted_verse(int num, const std::string& text, const std::string& query)
{
    if (query.empty())
    {
        ImGui::TextWrapped("%d. %s", num, text.c_str());
        return;
    }

    // Build lower case copies for matching
    std::string lower_text, lower_query;
    lower_text.reserve(text.size());
    for (auto c : text) lower_text += (char)tolower((unsigned char)c);
    for (auto c : query) lower_query += (char)tolower((unsigned char)c);

    ImGui::Text("%d. ", num);
    ImGui::SameLine(0, 0);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);

    size_t start = 0;
    while (start < text.size())
    {
        size_t pos = lower_text.find(lower_query, start);
        if (pos == std::string::npos)
        {
            ImGui::TextUnformatted(text.c_str() + start);
            break;
        }

        if (pos > start)
        {
            float py = ImGui::GetCursorPosY();
            ImGui::TextUnformatted(text.c_str() + start, text.c_str() + pos);
            if (ImGui::GetCursorPosY() == py)
                ImGui::SameLine(0, 0);
        }

        float py = ImGui::GetCursorPosY();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        ImGui::TextUnformatted(text.c_str() + pos, text.c_str() + pos + query.size());
        ImGui::PopStyleColor();
        if (ImGui::GetCursorPosY() == py)
            ImGui::SameLine(0, 0);

        start = pos + query.size();
    }

    ImGui::PopTextWrapPos();
}

static void render_passage(const std::vector<TestamentInfo>& data, int book_id, int chapter, int verse_num)
{
    for (auto& t : data)
    {
        for (auto& b : t.books)
        {
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
                        ImGui::SameLine();
                        bool bm = find_bookmark(g_data_entries, book_id, chapter, verse_num) >= 0;
                        if (ImGui::SmallButton(bm ? "Unbookmark" : "Bookmark"))
                        {
                            int idx = find_bookmark(g_data_entries, book_id, chapter, verse_num);
                            if (idx >= 0)
                                g_data_entries.erase(g_data_entries.begin() + idx);
                            else
                            {
                                DataEntry e;
                                e.type = "bookmark";
                                e.book_id = book_id;
                                e.chapter = chapter;
                                e.verse = verse_num;
                                e.sel_start = -1;
                                e.sel_end = -1;
                                std::string ts = timestamp();
                                e.created = ts;
                                e.modified = ts;
                                g_data_entries.push_back(e);
                            }
                        }
                        ImGui::BeginChild("##passage", ImVec2(-FLT_MIN, -FLT_MIN));
                        for (auto& v : c.verses)
                            if (verse_num == -1 || v.num == verse_num)
                                render_highlighted_verse(v.num, v.text, g_highlight_query);
                        ImGui::EndChild();
                        return;
                    }
                return;
            }
        }
    }
}

static int find_entry(const std::vector<DataEntry>& entries, int book, int chapter, int verse)
{
    for (size_t i = 0; i < entries.size(); i++)
    {
        auto& e = entries[i];
        if (e.type == "note" && e.book_id == book && e.chapter == chapter && e.verse == verse)
            return (int)i;
    }
    return -1;
}

static int find_bookmark(const std::vector<DataEntry>& entries, int book, int chapter, int verse)
{
    for (size_t i = 0; i < entries.size(); i++)
    {
        auto& e = entries[i];
        if (e.type == "bookmark" && e.book_id == book && e.chapter == chapter && e.verse == verse)
            return (int)i;
    }
    return -1;
}

static const char* book_name(const std::vector<TestamentInfo>& data, int id)
{
    for (auto& t : data)
        for (auto& b : t.books)
            if (b.id == id) return b.name.c_str();
    return "Unknown";
}

struct GoToSuggestion {
    std::string display;
    std::string insert_text;
};

struct GoToCbData {
    std::vector<GoToSuggestion>* suggestions;
    int* selected;
    bool* focus_requested;
};

static int go_to_callback(ImGuiInputTextCallbackData* data)
{
    GoToCbData* cb = (GoToCbData*)data->UserData;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
    {
        if (cb && cb->focus_requested && *cb->focus_requested)
        {
            data->CursorPos = data->BufTextLen;
            data->SelectionStart = data->SelectionEnd = data->BufTextLen;
            *cb->focus_requested = false;
        }
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
    {
        if (cb->selected && *cb->selected >= 0 && cb->suggestions && *cb->selected < (int)cb->suggestions->size())
        {
            auto& s = (*cb->suggestions)[*cb->selected];
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, s.insert_text.c_str());
            if (cb->focus_requested) *cb->focus_requested = true;
        }
    }
    return 0;
}

static std::vector<GoToSuggestion> get_go_to_suggestions(const char* input, const std::vector<TestamentInfo>& data)
{
    std::vector<GoToSuggestion> result;
    if (!input || !*input) return result;
    std::string s = input;

    // Check if input starts with a known book name
    std::string matched_book;
    std::string after_book;
    for (auto& t : data)
        for (auto& b : t.books)
        {
            if (b.name.size() > s.size()) continue;
            if (strncasecmp(b.name.c_str(), s.c_str(), b.name.size()) != 0) continue;
            if (s.size() > b.name.size() && s[b.name.size()] != ' ') continue;
            if (b.name.size() > matched_book.size())
            {
                matched_book = b.name;
                after_book = s.substr(b.name.size());
                size_t ns = after_book.find_first_not_of(' ');
                after_book = (ns != std::string::npos) ? after_book.substr(ns) : "";
            }
        }

    if (!matched_book.empty())
    {
        size_t colon = after_book.find(':');
        if (colon != std::string::npos)
        {
            // Verse suggestion mode
            std::string ch_str = after_book.substr(0, colon);
            std::string v_part = after_book.substr(colon + 1);
            int ch_num = atoi(ch_str.c_str());
            for (auto& t : data)
                for (auto& b : t.books)
                    if (strcasecmp(b.name.c_str(), matched_book.c_str()) == 0)
                        for (auto& c : b.chapters)
                            if (c.num == ch_num)
                                for (auto& v : c.verses)
                                {
                                    char vs[32];
                                    snprintf(vs, sizeof(vs), "%d", v.num);
                                    if (v_part.empty() || strncmp(vs, v_part.c_str(), v_part.size()) == 0)
                                    {
                                        char label[64];
                                        snprintf(label, sizeof(label), "Verse %d", v.num);
                                        result.push_back({label, matched_book + " " + ch_str + ":" + vs});
                                    }
                                }
        }
        else if (!after_book.empty())
        {
            // Chapter suggestion mode (filtered by typed digits)
            for (auto& t : data)
                for (auto& b : t.books)
                    if (strcasecmp(b.name.c_str(), matched_book.c_str()) == 0)
                        for (auto& c : b.chapters)
                        {
                            char cs[32];
                            snprintf(cs, sizeof(cs), "%d", c.num);
                            if (strncmp(cs, after_book.c_str(), after_book.size()) == 0)
                            {
                                char label[64];
                                snprintf(label, sizeof(label), "Chapter %d (%d v)", c.num, (int)c.verses.size());
                                result.push_back({label, matched_book + " " + cs + ":"});
                            }
                        }
        }
        else
        {
            // Just the book name → show all chapters
            for (auto& t : data)
                for (auto& b : t.books)
                    if (strcasecmp(b.name.c_str(), matched_book.c_str()) == 0)
                        for (auto& c : b.chapters)
                        {
                            char cs[32];
                            snprintf(cs, sizeof(cs), "%d", c.num);
                            char label[64];
                            snprintf(label, sizeof(label), "Chapter %d (%d v)", c.num, (int)c.verses.size());
                            result.push_back({label, matched_book + " " + cs + ":"});
                        }
        }
    }
    else
    {
        // Book suggestion mode
        for (auto& t : data)
            for (auto& b : t.books)
                if (strncasecmp(b.name.c_str(), s.c_str(), s.size()) == 0)
                {
                    char label[128];
                    snprintf(label, sizeof(label), "%s (%d ch)", b.name.c_str(), (int)b.chapters.size());
                    result.push_back({label, std::string(b.name) + " "});
                }
    }

    if (result.size() > 20) result.resize(20);
    return result;
}

static bool parse_reference(const char* input, const std::vector<TestamentInfo>& data, int& out_book, int& out_chapter, int& out_verse)
{
    std::string s = input;
    size_t colon = s.find(':');
    std::string before_verse = (colon != std::string::npos) ? s.substr(0, colon) : s;

    int best_id = -1;
    size_t best_len = 0;
    int chapter = 1;

    for (auto& t : data)
        for (auto& b : t.books)
        {
            if (b.name.size() <= best_len) continue;
            if (strncasecmp(b.name.c_str(), before_verse.c_str(), b.name.size()) != 0) continue;
            if (before_verse.size() > b.name.size() && before_verse[b.name.size()] != ' ') continue;

            std::string rest = before_verse.substr(b.name.size());
            size_t ns = rest.find_first_not_of(' ');
            rest = (ns != std::string::npos) ? rest.substr(ns) : "";

            int ch = 1;
            if (!rest.empty())
            {
                char* end = nullptr;
                ch = strtol(rest.c_str(), &end, 10);
                if (end == rest.c_str() || *end != '\0') continue;
            }

            best_id = b.id;
            best_len = b.name.size();
            chapter = ch;
        }

    // Fallback: unique prefix match for abbreviations
    if (best_id < 0)
    {
        int count = 0, id = -1;
        for (auto& t : data)
            for (auto& b : t.books)
                if (strncasecmp(b.name.c_str(), before_verse.c_str(), before_verse.size()) == 0)
                { count++; id = b.id; }
        if (count == 1) best_id = id;
    }

    if (best_id < 0) return false;
    out_book = best_id;
    out_chapter = chapter;
    out_verse = (colon != std::string::npos) ? atoi(s.substr(colon + 1).c_str()) : -1;
    return true;
}

static void flush_note(int book, int chapter, int verse, const char* buf)
{
    if (book < 0) return;
    std::string content = buf;
    int idx = find_entry(g_data_entries, book, chapter, verse);
    if (content.empty())
    {
        if (idx >= 0) g_data_entries.erase(g_data_entries.begin() + idx);
    }
    else
    {
        std::string ts = timestamp();
        if (idx >= 0)
        {
            g_data_entries[idx].content = content;
            g_data_entries[idx].modified = ts;
        }
        else
        {
            DataEntry e;
            e.type = "note";
            e.book_id = book;
            e.chapter = chapter;
            e.verse = verse;
            e.sel_start = -1;
            e.sel_end = -1;
            e.content = content;
            e.created = ts;
            e.modified = ts;
            g_data_entries.push_back(e);
        }
    }
    g_notes_tree_dirty = true;
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
    static bool allow_undock = false;
    static bool reset_layout = false;

    // Scan available translations
    static std::vector<std::string> g_translations;
    static bool g_translations_scanned = false;
    if (!g_translations_scanned)
    {
        g_translations_scanned = true;
        DIR* dir = opendir((exe_dir() + "/translations").c_str());
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

    // Notes windows state
    static bool show_notes = false;
    static bool show_notes_explorer = false;
    static char note_edit_buf[65536] = "";
    static int note_book = -1;
    static int note_chapter = -1;
    static int note_verse = -1;

    static const char* filters[] = {"*.json"};

    // Search state
    static bool show_go_to_dialog = false;
    static char go_to_buf[256] = "";
    static int go_to_sel = -1;
    static bool go_to_focus = false;
    static bool show_search = false;
    static bool show_bookmarks_dialog = false;
    static bool show_history_dialog = false;
    static bool show_special_search_dialog = false;
    static char search_buf[256] = "";
    static std::vector<SearchResult> search_results;

    // Navigation history
    struct HistoryEntry { int book; int chapter; int verse; std::string label; };
    static std::vector<HistoryEntry> nav_history;
    static int prev_nav_book = 1, prev_nav_chapter = 1, prev_nav_verse = -1;

    // Extra windows
    struct ExtraWin { bool open = false; std::string translation = "asv"; };
    static ExtraWin translation_windows[16];

    // Navigation state (driven by treeview)
    static int nav_book = 1;
    static int nav_chapter = 1;
    static int nav_verse = -1;  // -1 means start from verse 1

    

    // Load custom state
    {
        std::string state_path = exe_dir() + "/tomewell_state.ini";
        FILE* f = fopen(state_path.c_str(), "r");
        if (f)
        {
            char buf[1024];
            std::string section;
            while (fgets(buf, sizeof(buf), f))
            {
                buf[strcspn(buf, "\r\n")] = 0;
                if (strlen(buf) == 0) continue;
                if (buf[0] == '[')
                {
                    section = buf;
                    continue;
                }
                if (section == "[default_translation]")
                {
                    if (strlen(buf) > 0) def_translat = buf;
                }
                else if (section.find("[translation_window") == 0)
                {
                    int idx = 0;
                    sscanf(section.c_str(), "[translation_window%d]", &idx);
                    if (idx >= 0 && idx < 16)
                    {
                        int open = 0;
                        char trans[64] = "";
                        sscanf(buf, "%d %63s", &open, trans);
                        translation_windows[idx].open = (open != 0);
                        translation_windows[idx].translation = (strlen(trans) > 0) ? trans : "asv";
                    }
                }
                else if (section == "[navigation]")
                {
                    int b = 0, c = 0, v = 0;
                    if (sscanf(buf, "%d %d %d", &b, &c, &v) >= 2)
                    {
                        nav_book = b;
                        nav_chapter = c;
                        nav_verse = (v > 0) ? v : -1;
                    }
                }
                else if (section == "[show_notes]")
                {
                    int n = 0;
                    if (sscanf(buf, "%d", &n) == 1) show_notes = (n != 0);
                }
                else if (section == "[show_notes_explorer]")
                {
                    int n = 0;
                    if (sscanf(buf, "%d", &n) == 1) show_notes_explorer = (n != 0);
                }
                else if (section == "[data_path]")
                {
                    if (strlen(buf) > 0) g_data_path = buf;
                }
                else if (section == "[show_menu]")
                {
                    int n = 0;
                    if (sscanf(buf, "%d", &n) == 1) show_menu = (n != 0);
                }
                else if (section == "[allow_undock]")
                {
                    int n = 0;
                    if (sscanf(buf, "%d", &n) == 1) allow_undock = (n != 0);
                }
                else if (section == "[show_history]")
                {
                    int n = 0;
                    if (sscanf(buf, "%d", &n) == 1) show_history_dialog = (n != 0);
                }
                else if (section == "[bookmarks]")
                {
                    int n = 0;
                    if (sscanf(buf, "%d", &n) == 1) show_bookmarks_dialog = (n != 0);
                }
            }
            fclose(f);
        }
    }

    // Load/create data file
    {
        if (g_data_path.empty())
            g_data_path = exe_dir() + "/notes.json";
        std::ifstream test(g_data_path);
        if (!test.is_open())
            create_default_data_file(g_data_path);
        else
            test.close();
        g_data_entries = load_data_file(g_data_path);
    }

    // Main loop
#ifndef __EMSCRIPTEN__
    {
        static std::string ini_path = exe_dir() + "/imgui.ini";
        io.IniFilename = ini_path.c_str();
    }
#endif
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

        // Toggle multi-viewport for undocking (runtime-safe in ImGui)
        if (allow_undock)
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        else
            io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Track navigation history
        {
            static bool nav_history_first = true;
            if (nav_history_first)
            {
                HistoryEntry he;
                he.book = nav_book; he.chapter = nav_chapter; he.verse = nav_verse;
                const auto& hd = get_translation(def_translat);
                const char* bn = book_name(hd, nav_book);
                if (nav_verse >= 0)
                {
                    char lbl[256]; snprintf(lbl, sizeof(lbl), "%s %d:%d", bn, nav_chapter, nav_verse);
                    he.label = lbl;
                }
                else
                {
                    char lbl[256]; snprintf(lbl, sizeof(lbl), "%s Chapter %d", bn, nav_chapter);
                    he.label = lbl;
                }
                for (int j = (int)nav_history.size() - 1; j >= 0; j--)
                    if (nav_history[j].book == he.book && nav_history[j].chapter == he.chapter && nav_history[j].verse == he.verse)
                        nav_history.erase(nav_history.begin() + j);
                nav_history.push_back(he);
                prev_nav_book = nav_book; prev_nav_chapter = nav_chapter; prev_nav_verse = nav_verse;
                nav_history_first = false;
            }
            if (nav_book != prev_nav_book || nav_chapter != prev_nav_chapter || nav_verse != prev_nav_verse)
            {
                HistoryEntry he;
                he.book = nav_book; he.chapter = nav_chapter; he.verse = nav_verse;
                const auto& hd = get_translation(def_translat);
                const char* bn = book_name(hd, nav_book);
                if (nav_verse >= 0)
                {
                    char lbl[256]; snprintf(lbl, sizeof(lbl), "%s %d:%d", bn, nav_chapter, nav_verse);
                    he.label = lbl;
                }
                else
                {
                    char lbl[256]; snprintf(lbl, sizeof(lbl), "%s Chapter %d", bn, nav_chapter);
                    he.label = lbl;
                }
                for (int j = (int)nav_history.size() - 1; j >= 0; j--)
                    if (nav_history[j].book == he.book && nav_history[j].chapter == he.chapter && nav_history[j].verse == he.verse)
                        nav_history.erase(nav_history.begin() + j);
                nav_history.push_back(he);
                prev_nav_book = nav_book; prev_nav_chapter = nav_chapter; prev_nav_verse = nav_verse;
            }
            if (nav_history.size() > 100)
                nav_history.erase(nav_history.begin());
        }

        // Key Handlers
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_Q))
        {
            glfwSetWindowShouldClose(window, true);
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_M))
        {
            show_menu = !show_menu;
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_F))
        {
            show_search = !show_search;
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_H))
        {
            show_history_dialog = !show_history_dialog;
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_G))
        {
            show_go_to_dialog = true;
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_N))
        {
            flush_note(note_book, note_chapter, note_verse, note_edit_buf);
            const char* fp = tinyfd_saveFileDialog("New Database", g_data_path.c_str(), 1, filters, "JSON files");
            if (fp)
            {
                g_data_path = fp;
                g_data_entries.clear();
                note_edit_buf[0] = '\0';
                note_book = note_chapter = note_verse = -1;
                g_notes_tree_dirty = true;
                save_data_file(g_data_path, g_data_entries);
            }
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_N))
        {
            show_notes = true;
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_X))
        {
            show_notes_explorer = true;
        }

        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_O))
        {
            const char* fp = tinyfd_openFileDialog("Open Database", "", 1, filters, "JSON files", 0);
            if (fp)
            {
                flush_note(note_book, note_chapter, note_verse, note_edit_buf);
                note_edit_buf[0] = '\0';
                std::ifstream t(fp);
                if (t.is_open())
                {
                    t.close();
                    g_data_entries = load_data_file(fp);
                    g_data_path = fp;
                    note_book = note_chapter = note_verse = -1;
                    g_notes_tree_dirty = true;
                }
            }
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            flush_note(note_book, note_chapter, note_verse, note_edit_buf);
            save_data_file(g_data_path, g_data_entries);
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_D))
        {
            show_bookmarks_dialog = !show_bookmarks_dialog;
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && !ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_D))
        {
            int idx = find_bookmark(g_data_entries, nav_book, nav_chapter, nav_verse);
            if (idx >= 0)
                g_data_entries.erase(g_data_entries.begin() + idx);
            else
            {
                DataEntry e;
                e.type = "bookmark";
                e.book_id = nav_book;
                e.chapter = nav_chapter;
                e.verse = nav_verse;
                e.sel_start = -1;
                e.sel_end = -1;
                std::string ts = timestamp();
                e.created = ts;
                e.modified = ts;
                g_data_entries.push_back(e);
            }
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            flush_note(note_book, note_chapter, note_verse, note_edit_buf);
            const char* fp = tinyfd_saveFileDialog("Save Database As", g_data_path.c_str(), 1, filters, "JSON files");
            if (fp)
            {
                g_data_path = fp;
                save_data_file(g_data_path, g_data_entries);
            }
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_C))
        {
            g_expand_all = false;
            g_collapse_all = true;
        }
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_E))
        {
            g_expand_all = true;
            g_collapse_all = false;
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
                if (ImGui::MenuItem("Show Notes", "Ctrl+Shift+N"))
                {
                    show_notes = true;
                }
                ImGui::Separator();

                if (ImGui::MenuItem("New Database", "Ctrl+N"))
                {
                    flush_note(note_book, note_chapter, note_verse, note_edit_buf);
                    const char* fp = tinyfd_saveFileDialog("New Database", g_data_path.c_str(), 1, filters, "JSON files");
                    if (fp)
                    {
                        g_data_path = fp;
                        g_data_entries.clear();
                        note_edit_buf[0] = '\0';
                        note_book = note_chapter = note_verse = -1;
                        g_notes_tree_dirty = true;
                        save_data_file(g_data_path, g_data_entries);
                    }
                }
                if (ImGui::MenuItem("Open Database", "Ctrl+O"))
                {
                    const char* fp = tinyfd_openFileDialog("Open Database", "", 1, filters, "JSON files", 0);
                    if (fp)
                    {
                        flush_note(note_book, note_chapter, note_verse, note_edit_buf);
                        note_edit_buf[0] = '\0';
                        std::ifstream t(fp);
                        if (t.is_open())
                        {
                            t.close();
                            g_data_entries = load_data_file(fp);
                            g_data_path = fp;
                            note_book = note_chapter = note_verse = -1;
                            g_notes_tree_dirty = true;
                        }
                    }
                }
                if (ImGui::MenuItem("Save Database", "Ctrl+S"))
                {
                    flush_note(note_book, note_chapter, note_verse, note_edit_buf);
                    save_data_file(g_data_path, g_data_entries);
                }
                if (ImGui::MenuItem("Save Database As", "Ctrl+Shift+S"))
                {
                    flush_note(note_book, note_chapter, note_verse, note_edit_buf);
                    const char* fp = tinyfd_saveFileDialog("Save Database As", g_data_path.c_str(), 1, filters, "JSON files");
                    if (fp)
                    {
                        g_data_path = fp;
                        save_data_file(g_data_path, g_data_entries);
                    }
                }
                if (ImGui::MenuItem("Show Notes Explorer", "Ctrl+Shift+X"))
                {
                    show_notes_explorer = true;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Bookmark Verse", "Ctrl+D"))
                {
                    int idx = find_bookmark(g_data_entries, nav_book, nav_chapter, nav_verse);
                    if (idx >= 0)
                        g_data_entries.erase(g_data_entries.begin() + idx);
                    else
                    {
                        DataEntry e;
                        e.type = "bookmark";
                        e.book_id = nav_book;
                        e.chapter = nav_chapter;
                        e.verse = nav_verse;
                        e.sel_start = -1;
                        e.sel_end = -1;
                        std::string ts = timestamp();
                        e.created = ts;
                        e.modified = ts;
                        g_data_entries.push_back(e);
                    }
                }
                if (ImGui::MenuItem("Show Bookmarks", "Ctrl+Shift+D"))
                {
                    show_bookmarks_dialog = !show_bookmarks_dialog;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Quit", "Ctrl+Q"))
                {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Expand All", "Ctrl+Shift+E"))
                {
                    g_expand_all = true;
                    g_collapse_all = false;
                }
                if (ImGui::MenuItem("Collapse All", "Ctrl+Shift+C"))
                {
                    g_expand_all = false;
                    g_collapse_all = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Search"))
            {
                if (ImGui::MenuItem("Search Bible", "Ctrl+F"))
                {
                    show_search = true;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Go To Reference", "Ctrl+G"))
                {
                    show_go_to_dialog = true;
                }

                if (ImGui::MenuItem("Navigation History", "Ctrl+H"))
                {
                    show_history_dialog = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::MenuItem("Toggle Menu", "Ctrl+M"))
                {
                    show_menu = !show_menu;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Enable Undocking", nullptr, &allow_undock))
                {
                }

                ImGui::Separator();

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
                    show_notes = false;
                    show_notes_explorer = false;
                    show_search = false;
                    show_bookmarks_dialog = false;
                    show_history_dialog = false;

                    g_data_path = exe_dir() + "/notes.json";
                    remove((exe_dir() + "/tomewell_state.ini").c_str());
                    reset_layout = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("Tomewell Help"))
                {
                    std::string help_path = exe_dir() + "/tomewell_help.html";
#ifdef _WIN32
                    std::string cmd = "start \"\" \"" + help_path + "\"";
#else
                    std::string cmd = "xdg-open \"" + help_path + "\"";
#endif
                    system(cmd.c_str());
                }
                if (ImGui::MenuItem("Special Search Commands"))
                {
                    show_special_search_dialog = true;
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
            remove((exe_dir() + "/imgui.ini").c_str());
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
            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
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

        // Search window
        if (show_search)
        {
            ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(1.0f);
            if (ImGui::Begin("Search Bible", &show_search))
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                {
                    show_search = false;
                    search_buf[0] = '\0';
                    search_results.clear();
                    g_highlight_query.clear();
                    show_search = false;
                }

                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    search_buf[0] = '\0';
                    search_results.clear();
                }

                bool cur_is_regex = (strlen(search_buf) >= 2 && (search_buf[0] == 'r' || search_buf[0] == 'R') && search_buf[1] == ':');
                ImGui::InputText("Query", search_buf, sizeof(search_buf));
                if (cur_is_regex)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("[REGEX]");
                }
                {
                    static std::string prev_query;
                    std::string cur = search_buf;
                    if (cur != prev_query)
                    {
                        prev_query = cur;
                        search_results.clear();
                        g_highlight_query.clear();
                        g_regex_error.clear();
                        if (!cur.empty())
                        {
                            const auto& data = get_translation(def_translat);
                            bool is_regex = (cur.size() >= 2 && (cur[0] == 'r' || cur[0] == 'R') && cur[1] == ':');
                            std::string raw_query = is_regex ? cur.substr(2) : cur;
                            g_highlight_query = raw_query;
                            if (is_regex)
                            {
                                try
                                {
                                    std::regex pattern(raw_query, std::regex::icase | std::regex::optimize);
                                    for (auto& t : data)
                                        for (auto& b : t.books)
                                            for (auto& c : b.chapters)
                                                for (auto& v : c.verses)
                                                {
                                                    if (std::regex_search(v.text, pattern))
                                                    {
                                                        std::string snippet = v.text;
                                                        if (snippet.size() > 120) snippet = snippet.substr(0, 117) + "...";
                                                        search_results.push_back({b.id, b.name, c.num, v.num, snippet});
                                                    }
                                                }
                                }
                                catch (std::regex_error& e)
                                {
                                    g_regex_error = e.what();
                                }
                            }
                            else
                            {
                                std::string query = cur;
                                for (auto& q : query) q = (char)tolower(q);
                                for (auto& t : data)
                                    for (auto& b : t.books)
                                        for (auto& c : b.chapters)
                                            for (auto& v : c.verses)
                                            {
                                                std::string lower = v.text;
                                                for (auto& ch : lower) ch = (char)tolower(ch);
                                                if (lower.find(query) != std::string::npos)
                                                {
                                                    std::string snippet = v.text;
                                                    if (snippet.size() > 120) snippet = snippet.substr(0, 117) + "...";
                                                    search_results.push_back({b.id, b.name, c.num, v.num, snippet});
                                                }
                                            }
                            }
                        }
                    }
                }

                if (!search_results.empty())
                {
                    ImGui::Separator();
                    ImGui::Text("%zu results", search_results.size());
                    ImGui::BeginChild("##results", ImVec2(-FLT_MIN, -FLT_MIN), true);
                    for (size_t i = 0; i < search_results.size(); i++)
                    {
                        auto& r = search_results[i];
                        char label[256];
                        snprintf(label, sizeof(label), "%s %d:%d  %s", r.book_name.c_str(), r.chapter, r.verse, r.snippet.c_str());
                        ImGui::PushID((int)i);
                        if (ImGui::Selectable(label))
                        {
                            nav_book = r.book_id;
                            nav_chapter = r.chapter;
                            nav_verse = r.verse;
                        }
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                        {
                            g_tree_inited = false;
                            g_scroll_to_verse = true;
                            search_buf[0] = '\0';
                            search_results.clear();
                            show_search = false;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                }
                else if (strlen(search_buf) > 0 && search_results.empty())
                {
                    if (!g_regex_error.empty())
                        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "Regex error: %s", g_regex_error.c_str());
                    else
                        ImGui::TextUnformatted("No results found.");
                }
            }
            ImGui::End();
        }
        if (!show_search)
            g_highlight_query.clear();

        // Go to Reference dialog
        if (show_go_to_dialog)
        {
            ImGui::SetNextWindowSize(ImVec2(420, 550), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(1.0f);
            if (ImGui::Begin("Go to Reference", &show_go_to_dialog))
            {
                if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape))
                    show_go_to_dialog = false;

                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    go_to_buf[0] = '\0';
                    go_to_sel = -1;
                }

                const auto& data = get_translation(def_translat);
                auto suggestions = get_go_to_suggestions(go_to_buf, data);

                // If focus was requested (after accepting a suggestion), return it to the input
                // Note: go_to_focus is cleared by the callback after placing cursor at end
                if (go_to_focus)
                    ImGui::SetKeyboardFocusHere();

                // Parse reference (used for Enter logic and preview)
                int book_id = -1, chapter = 1, verse = -1;
                bool valid = parse_reference(go_to_buf, data, book_id, chapter, verse);

                // Enter to accept selected suggestion (only when buffer is NOT a valid reference)
                bool enter_accepted = false;
                if (!suggestions.empty() && go_to_sel >= 0 && !valid && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
                {
                    strncpy(go_to_buf, suggestions[go_to_sel].insert_text.c_str(), sizeof(go_to_buf) - 1);
                    go_to_buf[sizeof(go_to_buf) - 1] = '\0';
                    go_to_focus = true;
                    enter_accepted = true;
                }

                // Input text with Tab completion and cursor-position callbacks
                GoToCbData cb_data = { &suggestions, &go_to_sel, &go_to_focus };
                bool enter_pressed = ImGui::InputText("##ref", go_to_buf, sizeof(go_to_buf),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCompletion,
                    go_to_callback, &cb_data);

                // Clamp selection to valid range
                if (go_to_sel < 0 || go_to_sel >= (int)suggestions.size())
                    go_to_sel = suggestions.empty() ? -1 : 0;

                // Keyboard navigation (always active when suggestions exist)
                if (!suggestions.empty())
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
                        go_to_sel = (go_to_sel <= 0) ? (int)suggestions.size() - 1 : go_to_sel - 1;
                    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
                        go_to_sel = (go_to_sel + 1) % (int)suggestions.size();
                }

                // Suggestion popup (always visible when suggestions exist)
                if (!suggestions.empty())
                {
                    float height = ImMin(200.0f, (float)suggestions.size() * ImGui::GetTextLineHeightWithSpacing());
                    ImGui::BeginChild("##suggestions", ImVec2(ImGui::GetItemRectSize().x, height), true);
                    for (int i = 0; i < (int)suggestions.size(); i++)
                    {
                        bool is_sel = (i == go_to_sel);
                        if (ImGui::Selectable(suggestions[i].display.c_str(), is_sel))
                        {
                            go_to_sel = i;
                            strncpy(go_to_buf, suggestions[i].insert_text.c_str(), sizeof(go_to_buf) - 1);
                            go_to_buf[sizeof(go_to_buf) - 1] = '\0';
                            go_to_focus = true;
                        }
                    }
                    ImGui::EndChild();
                }

                ImGui::Separator();

                // Re-parse after InputText (buffer may have changed from user typing)
                book_id = -1; chapter = 1; verse = -1;
                valid = parse_reference(go_to_buf, data, book_id, chapter, verse);

                // Navigation: Enter navigates when not consumed by suggestion accept
                if (enter_pressed && !enter_accepted && valid)
                {
                    nav_book = book_id;
                    nav_chapter = chapter;
                    nav_verse = verse;
                    g_tree_inited = false;
                    g_scroll_to_verse = true;
                    show_go_to_dialog = false;
                }

                if (valid)
                {
                    ImGui::Text("Navigate to:  %s %d:%d", book_name(data, book_id), chapter, verse >= 0 ? verse : 1);
                    if (ImGui::Button("Go") && !enter_accepted)
                    {
                        nav_book = book_id;
                        nav_chapter = chapter;
                        nav_verse = verse;
                        g_tree_inited = false;
                        g_scroll_to_verse = true;
                        show_go_to_dialog = false;
                    }
                }
                else if (strlen(go_to_buf) > 0)
                {
                    ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "Unrecognized reference");
                }
                else
                {
                    ImGui::TextDisabled("Type a book name, then chapter:verse");
                }
            }
            ImGui::End();
        }

        // Navigation history dialog
        if (show_history_dialog)
        {
            ImGui::SetNextWindowBgAlpha(1.0f);
            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
            ImGui::Begin("Navigation History", &show_history_dialog);
            {
                ImGui::Text("%zu entries", nav_history.size());
                ImGui::Separator();
                ImGui::BeginChild("##history", ImVec2(-FLT_MIN, -FLT_MIN), true);
                for (int i = (int)nav_history.size() - 1; i >= 0; i--)
                {
                    auto& h = nav_history[i];
                    ImGui::PushID(i);
                    if (ImGui::Selectable(h.label.c_str()))
                    {
                        nav_book = h.book;
                        nav_chapter = h.chapter;
                        nav_verse = h.verse;
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }

        // Notes editor
        if (show_notes)
        {
            const auto& tree_data = get_translation(def_translat);
            if (nav_book != note_book || nav_chapter != note_chapter || nav_verse != note_verse)
            {
                flush_note(note_book, note_chapter, note_verse, note_edit_buf);
                note_book = nav_book;
                note_chapter = nav_chapter;
                note_verse = nav_verse;
                int idx = find_entry(g_data_entries, note_book, note_chapter, note_verse);
                if (idx >= 0)
                {
                    const auto& c = g_data_entries[idx].content;
                    size_t len = c.size();
                    if (len >= sizeof(note_edit_buf)) len = sizeof(note_edit_buf) - 1;
                    std::memcpy(note_edit_buf, c.c_str(), len);
                    note_edit_buf[len] = '\0';
                }
                else
                {
                    note_edit_buf[0] = '\0';
                }
            }

            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
            ImGui::Begin("Notes Editor", &show_notes);
            {
                ImGui::TextDisabled("%s", g_data_path.c_str());

                const char* bn = book_name(tree_data, note_book);
                if (note_verse >= 0)
                    ImGui::Text("%s  %d:%d", bn, note_chapter, note_verse);
                else
                    ImGui::Text("%s  Chapter %d", bn, note_chapter);

                ImGui::Separator();

                ImGui::InputTextMultiline("##note", note_edit_buf, sizeof(note_edit_buf),
                    ImVec2(-FLT_MIN, -FLT_MIN));
            }
            ImGui::End();
        }

        // Notes Explorer
        if (show_notes_explorer)
        {
            const auto& notes_tree_data = get_translation(def_translat);
            if (g_notes_tree_dirty)
                rebuild_notes_tree(notes_tree_data);

            ImGui::SetNextWindowBgAlpha(1.0f);
            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
            ImGui::Begin("Notes Explorer", &show_notes_explorer);
            {
                for (auto& t : g_notes_tree)
                {
                    if (ImGui::TreeNodeEx(t.label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        for (auto& b : t.books)
                        {
                            ImGuiTreeNodeFlags b_flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                            if (b.id == nav_book)
                                b_flags |= ImGuiTreeNodeFlags_Selected;
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
                                            if (b.id == nav_book && c.num == nav_chapter && v.num == nav_verse)
                                                v_flags |= ImGuiTreeNodeFlags_Selected;
                                            ImGui::TreeNodeEx(v_label, v_flags);
                                            ImGui::PopID();
                                            if (ImGui::IsItemClicked())
                                            {
                                                nav_book = b.id;
                                                nav_chapter = c.num;
                                                nav_verse = v.num;
                                            }
                                        }
                                        if (c.has_chapter_note)
                                        {
                                            ImGuiTreeNodeFlags cn_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                                            if (b.id == nav_book && c.num == nav_chapter && nav_verse == -1)
                                                cn_flags |= ImGuiTreeNodeFlags_Selected;
                                            ImGui::TreeNodeEx("Chapter Note", cn_flags);
                                            if (ImGui::IsItemClicked())
                                            {
                                                nav_book = b.id;
                                                nav_chapter = c.num;
                                                nav_verse = -1;
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
        }

        // Bookmarks dialog
        if (show_bookmarks_dialog)
        {
            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
            ImGui::Begin("Bookmarks", &show_bookmarks_dialog);
            if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
                show_bookmarks_dialog = false;
            {
                const auto& bm_tree_data = get_translation(def_translat);
                bool any = false;
                for (size_t i = 0; i < g_data_entries.size(); i++)
                {
                    auto& e = g_data_entries[i];
                    if (e.type != "bookmark") continue;
                    any = true;
                    const char* bn = book_name(bm_tree_data, e.book_id);
                    if (e.verse >= 0)
                        ImGui::Text("%s %d:%d", bn, e.chapter, e.verse);
                    else
                        ImGui::Text("%s Chapter %d", bn, e.chapter);
                    ImGui::SameLine();
                    char go_id[32];
                    snprintf(go_id, sizeof(go_id), "Go##bm%d", (int)i);
                    if (ImGui::SmallButton(go_id))
                    {
                        nav_book = e.book_id;
                        nav_chapter = e.chapter;
                        nav_verse = e.verse;
                        g_tree_inited = false;
                        g_scroll_to_verse = true;
                    }
                    ImGui::SameLine();
                    char del_id[32];
                    snprintf(del_id, sizeof(del_id), "X##bm%d", (int)i);
                    if (ImGui::SmallButton(del_id))
                    {
                        g_data_entries.erase(g_data_entries.begin() + i);
                        i--;
                    }
                }
                if (!any)
                {
                    ImGui::TextDisabled("No bookmarks yet.");
                    ImGui::TextDisabled("Navigate to a verse and press Ctrl+D to bookmark it.");
                }
            }
            ImGui::End();
        }

        if (show_special_search_dialog)
        {
            ImGui::SetNextWindowBgAlpha(1.0f);
            ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
            ImGui::Begin("Special Search Commands", &show_special_search_dialog);
            if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
                show_special_search_dialog = false;
            {
                ImGui::TextDisabled("Prefix your search query with \"r:\" to use regex.");
                ImGui::Separator();
                ImGui::TextDisabled("BASIC MATCHING");
                ImGui::TextDisabled("cat");
                ImGui::TextDisabled("  Find text cat");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("cat|dog");
                ImGui::TextDisabled("  Find cat OR dog");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("ANY CHARACTER");
                ImGui::TextDisabled("c.t");
                ImGui::TextDisabled("  Matches:");
                ImGui::TextDisabled("    cat");
                ImGui::TextDisabled("    cut");
                ImGui::TextDisabled("    c9t");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("CHARACTER TYPES");
                ImGui::TextDisabled("\\d");
                ImGui::TextDisabled("  Any digit");
                ImGui::TextDisabled("  Example: 5");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\w");
                ImGui::TextDisabled("  Any letter, number, or _");
                ImGui::TextDisabled("  Example: abc_123");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\s");
                ImGui::TextDisabled("  Any whitespace");
                ImGui::TextDisabled("  Example: space or tab");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("[abc]");
                ImGui::TextDisabled("  Match a, b, or c");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("[a-z]");
                ImGui::TextDisabled("  Any lowercase letter");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("[A-Z]");
                ImGui::TextDisabled("  Any uppercase letter");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("[0-9]");
                ImGui::TextDisabled("  Any number");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("REPEATING");
                ImGui::TextDisabled("*");
                ImGui::TextDisabled("  Zero or more");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("+");
                ImGui::TextDisabled("  One or more");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("?");
                ImGui::TextDisabled("  Optional");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("Examples:");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\d+");
                ImGui::TextDisabled("  Any number");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("a+");
                ImGui::TextDisabled("  a, aa, aaa");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\w{5}");
                ImGui::TextDisabled("  Exactly 5 characters");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\d{2,4}");
                ImGui::TextDisabled("  Between 2 and 4 digits");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("LINE MATCHING");
                ImGui::TextDisabled("^text");
                ImGui::TextDisabled("  Starts with text");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("text$");
                ImGui::TextDisabled("  Ends with \"text\"");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("^\\d+$");
                ImGui::TextDisabled("  Entire line must be numbers");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("COMMON SEARCHES");
                ImGui::TextDisabled("\\d+");
                ImGui::TextDisabled("  Find numbers");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\bword\\b");
                ImGui::TextDisabled("  Find whole word only");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("(error|warning|fatal)");
                ImGui::TextDisabled("  Find any of these words");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\.\\w+$");
                ImGui::TextDisabled("  Find file extensions");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("https?://\\S+");
                ImGui::TextDisabled("  Find URLs");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("^\\s*$");
                ImGui::TextDisabled("  Find blank lines");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("ESCAPING SPECIAL CHARACTERS");
                ImGui::TextDisabled("Special characters:");
                ImGui::TextDisabled(". ^ $ * + ? ( ) [ ] { } \\ |");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("Use \\ to search them literally.");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("Example:");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\.");
                ImGui::TextDisabled("  Find a period");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("\\(");
                ImGui::TextDisabled("  Find (");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("TIPS");
                ImGui::TextDisabled(".   = any character");
                ImGui::TextDisabled("*   = zero or more");
                ImGui::TextDisabled("+   = one or more");
                ImGui::TextDisabled("?   = optional");
                ImGui::TextDisabled("\\d  = digit");
                ImGui::TextDisabled("\\w  = word character");
                ImGui::TextDisabled("\\s  = whitespace");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("Regex is usually case-sensitive.");
                ImGui::TextDisabled("Start simple and build from there.");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled(" ");
                ImGui::Separator();
                ImGui::TextDisabled("Example:   r:\\d+");
                ImGui::TextDisabled("Finds verses containing one or more digits.");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("Example:   r:and|or");
                ImGui::TextDisabled("Finds verses containing \\\"and\" or \"or\".");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("Example:   r:^Blessed");
                ImGui::TextDisabled("Finds verses starting with \\\"Blessed\\\".");
                ImGui::TextDisabled(" ");
                ImGui::TextDisabled("Example:   r:blessed$");
                ImGui::TextDisabled("Finds verses ending with \\\"blessed\\\".");
            }
            ImGui::End();
        }

        {
            ImGui::Begin("Treeview");

            const auto& tree_data = get_translation(def_translat);
            if (g_collapse_all)
            {
                for (auto& t : tree_data)
                {
                    ImGui::SetNextItemOpen(true);
                    if (ImGui::TreeNodeEx(t.label.c_str(), 0))
                    {
                        for (auto& b : t.books)
                        {
                            ImGui::SetNextItemOpen(true);
                            if (ImGui::TreeNodeEx(b.name.c_str(), 0))
                            {
                                for (auto& c : b.chapters)
                                {
                                    char ch_label[32];
                                    snprintf(ch_label, sizeof(ch_label), "Chapter %d", c.num);
                                    ImGui::SetNextItemOpen(false);
                                    ImGui::TreeNodeEx(ch_label, 0);
                                }
                                ImGui::TreePop();
                            }
                        }
                        ImGui::TreePop();
                    }
                }
                for (auto& t : tree_data)
                {
                    ImGui::SetNextItemOpen(true);
                    if (ImGui::TreeNodeEx(t.label.c_str(), 0))
                    {
                        for (auto& b : t.books)
                        {
                            ImGui::SetNextItemOpen(false);
                            ImGui::TreeNodeEx(b.name.c_str(), 0);
                        }
                        ImGui::TreePop();
                    }
                }
                for (auto& t : tree_data)
                {
                    ImGui::SetNextItemOpen(false);
                    ImGui::TreeNodeEx(t.label.c_str(), 0);
                }
            }

            for (auto& t : tree_data)
            {
                bool has_nav = false;
                for (auto& b : t.books)
                    if (b.id == nav_book) { has_nav = true; break; }
                if (!g_collapse_all && (g_expand_all || (!g_tree_inited && has_nav)))
                    ImGui::SetNextItemOpen(true);
                if (ImGui::TreeNodeEx(t.label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (auto& b : t.books)
                    {
                        ImGuiTreeNodeFlags b_flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (b.id == nav_book)
                        {
                            b_flags |= ImGuiTreeNodeFlags_Selected;
                            if (!g_collapse_all && !g_tree_inited) ImGui::SetNextItemOpen(true);
                        }
                        if (!g_collapse_all && g_expand_all)
                            ImGui::SetNextItemOpen(true);
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
                                {
                                    c_flags |= ImGuiTreeNodeFlags_Selected;
                                    if (!g_collapse_all && !g_tree_inited) ImGui::SetNextItemOpen(true);
                                }
                                if (!g_collapse_all && g_expand_all)
                                    ImGui::SetNextItemOpen(false);
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
                                        if (b.id == nav_book && c.num == nav_chapter && v.num == nav_verse)
                                            v_flags |= ImGuiTreeNodeFlags_Selected;
                                        ImGui::TreeNodeEx(v_label, v_flags);
                                        ImGui::PopID();
                                        if (b.id == nav_book && c.num == nav_chapter && v.num == nav_verse && g_scroll_to_verse)
                                        {
                                            ImGui::ScrollToItem();
                                            g_scroll_to_verse = false;
                                        }
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

            g_tree_inited = true;
            g_expand_all = false;
            g_collapse_all = false;

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
        // Must be called every frame (UpdatePlatformWindows self-guards when viewports are disabled)
        // to keep internal frame tracking in sync.
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

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


    // Flush and save notes
    flush_note(note_book, note_chapter, note_verse, note_edit_buf);
    save_data_file(g_data_path, g_data_entries);

    // Store custom state
    {
        FILE* f = fopen((exe_dir() + "/tomewell_state.ini").c_str(), "w");
        if (f)
        {
            fprintf(f, "[default_translation]\n%s\n", def_translat.c_str());
            for (int i = 0; i < 16; i++)
            {
                fprintf(f, "[translation_window%d]\n%d %s\n", i,
                    translation_windows[i].open ? 1 : 0,
                    translation_windows[i].translation.c_str());
            }
            fprintf(f, "[navigation]\n%d %d %d\n", nav_book, nav_chapter, nav_verse);
            fprintf(f, "[show_notes]\n%d\n", show_notes ? 1 : 0);
            fprintf(f, "[show_notes_explorer]\n%d\n", show_notes_explorer ? 1 : 0);
            fprintf(f, "[data_path]\n%s\n", g_data_path.c_str());
            fprintf(f, "[show_menu]\n%d\n", show_menu ? 1 : 0);
            fprintf(f, "[allow_undock]\n%d\n", allow_undock ? 1 : 0);
            fprintf(f, "[show_history]\n%d\n", show_history_dialog ? 1 : 0);
            fprintf(f, "[bookmarks]\n%d\n", show_bookmarks_dialog ? 1 : 0);
            fclose(f);
        }
    }

    return 0;
}
