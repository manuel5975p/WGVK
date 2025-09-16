#!/bin/bash
set -euo pipefail

# WGVK Release Build Script
# Detects build system and creates release artifacts

echo "🔧 WGVK Release Build Script"
echo "Platform: $(uname -s)"
echo "Architecture: $(uname -m)"

# Create output directories
mkdir -p dist artifacts

# Detect build system and build
if [[ -f "CMakeLists.txt" ]]; then
    echo "✅ Detected CMake project"
    
    # Configure for release build
    mkdir -p build-release
    cd build-release
    
    echo "🔧 Configuring CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DWGVK_BUILD_EXAMPLES=OFF \
        -DWGVK_USE_VMA=ON \
        -DWGVK_BUILD_GLSL_SUPPORT=ON \
        -DWGVK_BUILD_WGSL_SUPPORT=OFF
    
    echo "🔨 Building..."
    cmake --build . --config Release --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    cd ..
    
    # Copy artifacts
    echo "📦 Collecting artifacts..."
    
    # Find built libraries and copy them
    if [[ -f "build-release/libwgvk.a" ]]; then
        cp "build-release/libwgvk.a" dist/
        cp "build-release/libwgvk.a" artifacts/
        echo "✅ Static library: libwgvk.a"
    fi
    
    # Copy headers for distribution
    if [[ -d "include" ]]; then
        cp -r include dist/
        echo "✅ Headers copied to dist/include"
    fi
    
    # Create a simple usage example
    cat > dist/README.txt << EOF
WGVK Release Build

This package contains:
- libwgvk.a: Static library for linking
- include/: Header files

Basic usage:
gcc your_program.c -I./include -L. -lwgvk -o your_program

For more examples and documentation, visit:
https://github.com/manuel5975p/WGVK
EOF

    echo "✅ Build completed successfully"
    echo "📁 Artifacts in: $(pwd)/dist"
    ls -la dist/
    
elif [[ -f "Cargo.toml" ]]; then
    echo "✅ Detected Rust project"
    cargo build --release
    # Copy binary from target/release/
    find target/release/ -maxdepth 1 -type f -executable ! -name "*.d" -exec cp {} dist/ \;
    
elif [[ -f "go.mod" ]]; then
    echo "✅ Detected Go project"
    go build -o dist/ ./...
    
elif [[ -f "package.json" ]]; then
    echo "✅ Detected Node.js project"
    npm ci
    npm run build 2>/dev/null || echo "No build script found"
    # Try to find built files
    if [[ -d "dist" ]]; then
        echo "Found dist directory"
    elif [[ -d "build" ]]; then
        cp -r build/* dist/ 2>/dev/null || true
    fi
    
elif [[ -f "Makefile" ]]; then
    echo "✅ Detected Makefile project"
    make release 2>/dev/null || make all 2>/dev/null || make
    # Try to find built binaries
    find . -maxdepth 2 -type f -executable ! -path "./.*" -exec cp {} dist/ \; 2>/dev/null || true
    
else
    echo "❌ No supported build system detected"
    echo "Expected one of: CMakeLists.txt, Cargo.toml, go.mod, package.json, Makefile"
    echo ""
    echo "For custom build systems, modify this script at:"
    echo "scripts/release/build.sh"
    exit 1
fi

echo ""
echo "🎉 Build script completed"
echo "📁 Output directory contents:"
ls -la dist/ 2>/dev/null || echo "No dist directory created"
ls -la artifacts/ 2>/dev/null || echo "No artifacts directory created"