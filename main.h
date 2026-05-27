#ifndef MAIN_H
#define MAIN_H

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <dirent.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <libgen.h>
#endif
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "nlohmann/json.hpp"

struct VerseInfo { int num; std::string text; };
struct ChapterInfo { int num; std::vector<VerseInfo> verses; };
struct BookInfo { int id; std::string name; std::vector<ChapterInfo> chapters; };
struct TestamentInfo { int num; std::string label; std::vector<BookInfo> books; };

struct SearchResult { int book_id; std::string book_name; int chapter; int verse; std::string snippet; };

struct DataEntry {
    std::string type;       // "note", future: "bookmark", etc.
    int book_id;
    int chapter;
    int verse;              // -1 for chapter-level
    int sel_start;          // -1 for non-selection
    int sel_end;            // -1 for non-selection
    std::string content;
    std::string created;
    std::string modified;
};

struct StudyNode {
    int id;
    int parent_id; // -1 for root
    std::string title;
    std::string type; // "folder" or "study"
    std::string content;
    int sort_order;
};

std::vector<TestamentInfo> load_translation(const std::string& path);
std::string exe_dir();
std::vector<DataEntry> load_data_file(const std::string& path, std::vector<StudyNode>* studies = nullptr);
void save_data_file(const std::string& path, const std::vector<DataEntry>& entries, const std::vector<StudyNode>& studies = {});
void create_default_data_file(const std::string& path);
std::string timestamp();

#endif
