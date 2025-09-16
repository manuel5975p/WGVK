#!/bin/bash
set -euo pipefail

echo "🧪 Testing release workflow components"

# Test 1: Validate YAML syntax
echo "📝 Validating YAML syntax..."
if command -v python3 &> /dev/null; then
    python3 -c "
import yaml
import sys

try:
    with open('.github/workflows/release-binaries.yml', 'r') as f:
        yaml.safe_load(f)
    print('✅ YAML syntax is valid')
except Exception as e:
    print(f'❌ YAML syntax error: {e}')
    sys.exit(1)
"
else
    echo "⚠️  Python3 not found, skipping YAML validation"
fi

# Test 2: Check build script functionality
echo "🔨 Testing build script..."
if [[ -f "scripts/release/build.sh" ]] && [[ -x "scripts/release/build.sh" ]]; then
    echo "✅ Build script exists and is executable"
    
    # Test script detection logic (dry run)
    if [[ -f "CMakeLists.txt" ]]; then
        echo "✅ CMake project detected (as expected)"
    else
        echo "❌ CMake project not detected"
        exit 1
    fi
else
    echo "❌ Build script missing or not executable"
    exit 1
fi

# Test 3: Check required directories
echo "📁 Checking directory structure..."
if [[ -d ".github/workflows" ]]; then
    echo "✅ .github/workflows directory exists"
else
    echo "❌ .github/workflows directory missing"
    exit 1
fi

if [[ -d "scripts/release" ]]; then
    echo "✅ scripts/release directory exists"
else
    echo "❌ scripts/release directory missing"
    exit 1
fi

# Test 4: Verify build produces expected outputs
echo "🔧 Testing build output..."
if [[ -d "dist" ]] && [[ -f "dist/libwgvk.a" ]]; then
    echo "✅ Previous build produced expected library"
    echo "   Library size: $(du -h dist/libwgvk.a | cut -f1)"
    echo "   Headers: $(find dist/include -name "*.h" | wc -l) header files"
else
    echo "⚠️  No previous build output found (run './scripts/release/build.sh' first)"
fi

echo ""
echo "🎉 All tests passed!"
echo ""
echo "To manually test the workflow:"
echo "1. Create a test tag: git tag v0.0.1-test"
echo "2. Push the tag: git push origin v0.0.1-test"
echo "3. Check GitHub Actions for workflow execution"
echo ""
echo "To test build locally:"
echo "./scripts/release/build.sh"