================================================================================
  Tomewell - Windows Build & Run Guide
================================================================================

Two options to get tomewell.exe running on Windows:

OPTION 1: Grab the pre-built .exe from the Linux build
-------------------------------------------------------------------------------
Run build_windows.sh on Linux to produce bin/windows/tomewell.exe, then copy
it to your Windows machine.  The .exe is statically linked so you only need:

  - A GPU with OpenGL 3.0+ drivers (built-in on any modern Windows)

That's it -- just double-click tomewell.exe.

================================================================================

OPTION 2: Build directly on Windows (MSYS2 / MinGW-w64)
-------------------------------------------------------------------------------
1. Install MSYS2 from https://www.msys2.org/

2. Open "MSYS2 MINGW64" from the Start menu and run:

      pacman -Syu                           # update everything
      pacman -S --needed mingw-w64-x86_64-toolchain
      pacman -S --needed mingw-w64-x86_64-glfw

3. Clone the repo and build:

      git clone https://github.com/baiguai/tomewell.git
      cd tomewell
      mingw32-make                          # produces tomewell.exe

================================================================================

PREREQUISITES (what each piece is)
-------------------------------------------------------------------------------
  MinGW-w64    -- GCC compiler suite for Windows (g++ for .exe files)
  GLFW         -- Window/OpenGL context library
  Dear ImGui   -- GUI library (bundled in deps/imgui/ within the repo)

================================================================================

TROUBLESHOOTING
-------------------------------------------------------------------------------
"libstdc++-6.dll was not found"
  -> The .exe was not statically linked.  Rebuild with build_windows.sh
     (it now uses -static-libstdc++ -static-libgcc) or install MinGW-w64
     and add its bin/ directory to your PATH.

No .exe / nothing happens when double-clicking
  -> Open a Command Prompt in the same folder and run tomewell.exe to see
     any error messages.
