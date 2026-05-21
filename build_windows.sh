#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="$SCRIPT_DIR/deps"
IMGUI_DIR="$DEPS_DIR/imgui"
GLFW_DIR="$DEPS_DIR/glfw-mingw"
BIN_DIR="$SCRIPT_DIR/bin/windows"
EXE="scriptorioc.exe"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

CXX="x86_64-w64-mingw32-g++"
CC="x86_64-w64-mingw32-gcc"
BASE_FLAGS="-O2 -I$IMGUI_DIR -I$IMGUI_DIR/backends -I$GLFW_DIR/include -I$DEPS_DIR -I$DEPS_DIR/tinyfiledialogs"
CXXFLAGS="-std=c++11 $BASE_FLAGS"
CFLAGS="$BASE_FLAGS"
CXXFLAGS_STATIC="-static-libstdc++ -static-libgcc"
LIBS="-L$GLFW_DIR/lib-mingw-w64 -lglfw3 -lgdi32 -lopengl32 -limm32 -lole32 -lcomdlg32"

SOURCES=(
    main.cpp
    "$DEPS_DIR/tinyfiledialogs/tinyfiledialogs.c"
    "$IMGUI_DIR/imgui.cpp"
    "$IMGUI_DIR/imgui_demo.cpp"
    "$IMGUI_DIR/imgui_draw.cpp"
    "$IMGUI_DIR/imgui_tables.cpp"
    "$IMGUI_DIR/imgui_widgets.cpp"
    "$IMGUI_DIR/backends/imgui_impl_glfw.cpp"
    "$IMGUI_DIR/backends/imgui_impl_opengl3.cpp"
)

echo -e "${YELLOW}==> Checking for MinGW-w64...${NC}"
if ! command -v "$CXX" &>/dev/null; then
    echo -e "${RED}$CXX not found. Install it with:${NC}"
    echo "  sudo apt install mingw-w64"
    echo ""
    read -rp "Install mingw-w64 now? [y/N] " answer
    if [[ "$answer" =~ ^[Yy]$ ]]; then
        sudo apt install -y mingw-w64
    else
        echo "Aborting."
        exit 1
    fi
fi

echo -e "${YELLOW}==> Checking for GLFW (MinGW)...${NC}"
if [[ ! -f "$GLFW_DIR/lib-mingw-w64/libglfw3.a" ]]; then
    echo "  Downloading pre-built GLFW for MinGW-w64..."
    mkdir -p "$GLFW_DIR"
    GLFW_URL="https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip"
    TMPZIP=$(mktemp)
    curl -fsSL "$GLFW_URL" -o "$TMPZIP"
    unzip -q -o "$TMPZIP" -d "$DEPS_DIR"
    mv "$DEPS_DIR/glfw-3.4.bin.WIN64"/* "$GLFW_DIR"
    rm -rf "$DEPS_DIR/glfw-3.4.bin.WIN64" "$TMPZIP"
    echo -e "${GREEN}  GLFW for MinGW downloaded.${NC}"
else
    echo -e "${GREEN}  GLFW found at $GLFW_DIR${NC}"
fi

echo -e "${YELLOW}==> Compiling...${NC}"
mkdir -p "$BIN_DIR"
FULL_EXE="$BIN_DIR/$EXE"
OBJS=()
for src in "${SOURCES[@]}"; do
    base="$(basename "$src")"
    obj="${base%.*}.o"
    ext="${src##*.}"
    if [[ "$ext" == "c" ]]; then
        echo "  $CC -c $src"
        $CC $CFLAGS -c "$src" -o "$obj"
    else
        echo "  $CXX -c $src"
        $CXX $CXXFLAGS $CXXFLAGS_STATIC -c "$src" -o "$obj"
    fi
    OBJS+=("$obj")
done

echo -e "${YELLOW}==> Linking...${NC}"
$CXX $CXXFLAGS_STATIC -o "$FULL_EXE" "${OBJS[@]}" $LIBS

echo -e "${YELLOW}==> Cleaning up .o files...${NC}"
rm -f "${OBJS[@]}"

echo -e "${YELLOW}==> Copying translations...${NC}"
rm -rf "$BIN_DIR/translations"
rsync -a --exclude='*.py' "$SCRIPT_DIR/translations/done/" "$BIN_DIR/translations/"

echo -e "${YELLOW}==> Copying help file...${NC}"
cp "$SCRIPT_DIR/scriptorioc_help.html" "$BIN_DIR/"

echo -e "${GREEN}==> Done: $FULL_EXE${NC}"
echo -e "${GREEN}    (copy the whole bin/windows/ folder to a Windows machine to run)${NC}"
