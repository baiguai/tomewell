#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="$SCRIPT_DIR/deps"
IMGUI_DIR="$DEPS_DIR/imgui"
EXE="tomewell"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}==> Checking dependencies...${NC}"

if ! pkg-config --exists glfw3 2>/dev/null; then
    echo -e "${RED}glfw3 not found. Install it with:${NC}"
    echo "  sudo apt install libglfw3-dev"
    echo ""
    read -rp "Install libglfw3-dev now? [y/N] " answer
    if [[ "$answer" =~ ^[Yy]$ ]]; then
        sudo apt install -y libglfw3-dev
    else
        echo "Aborting."
        exit 1
    fi
fi

if [[ ! -f "$IMGUI_DIR/imgui.h" ]]; then
    echo -e "${YELLOW}==> Fetching imgui (docking branch)...${NC}"
    mkdir -p "$DEPS_DIR"
    git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git "$IMGUI_DIR"
else
    echo -e "${GREEN}==> imgui source found at $IMGUI_DIR${NC}"
fi

echo -e "${YELLOW}==> Building...${NC}"
make -C "$SCRIPT_DIR" IMGUI_DIR="$IMGUI_DIR" EXE="$EXE" clean 2>/dev/null || true
make -C "$SCRIPT_DIR" IMGUI_DIR="$IMGUI_DIR" EXE="$EXE" -j"$(nproc)"

echo -e "${GREEN}==> Build complete: $SCRIPT_DIR/$EXE${NC}"
