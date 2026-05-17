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
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

struct VerseInfo { int num; std::string text; };
struct ChapterInfo { int num; std::vector<VerseInfo> verses; };
struct BookInfo { int id; std::string name; std::vector<ChapterInfo> chapters; };
struct TestamentInfo { int num; std::string label; std::vector<BookInfo> books; };

std::vector<TestamentInfo> load_translation(const std::string& path);

#endif
