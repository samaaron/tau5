#!/bin/bash
set -e # Quit script on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
WORKING_DIR="$(pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."

cleanup_function() {
    # Restore working directory as it was prior to this script running on exit
    cd "${WORKING_DIR}"
}
trap cleanup_function EXIT

echo "================================================"
echo "Tau5 macOS Release Build Fix"
echo "================================================"
echo ""
echo "This script fixes SSL dynamic library dependencies and symlinks"
echo "in the release build. Run this after build-release.sh"
echo ""

# Check that release directory exists
if [ ! -d "${ROOT_DIR}/release" ]; then
    echo "ERROR: release directory not found at ${ROOT_DIR}/release"
    echo "Make sure you've run build-release.sh first."
    exit 1
fi

# Find the release app bundle
cd "${ROOT_DIR}/release"
RELEASE_APP_DIR=$(find . -name "*.app" -type d | head -n 1)

if [ -z "$RELEASE_APP_DIR" ]; then
    echo "ERROR: Could not find app bundle in release directory!"
    echo "Make sure you've run build-release.sh first."
    exit 1
fi

# Get absolute path for the app
RELEASE_APP_PATH="${ROOT_DIR}/release/${RELEASE_APP_DIR}"

echo "Found app bundle: $RELEASE_APP_DIR"
echo ""

# Fix OpenSSL/crypto dynamic library dependencies
echo "================================================"
echo "Step 1: Fixing OpenSSL/crypto dynamic library dependencies..."
echo "================================================"

# Navigate to the crypto library location - use a loop to find it
CRYPTO_LIB_DIR=""
for dir in "${RELEASE_APP_PATH}"/Contents/Resources/_build/prod/rel/*/lib/crypto-*/priv/lib; do
    if [ -d "$dir" ]; then
        CRYPTO_LIB_DIR="$dir"
        break
    fi
done

if [ -n "$CRYPTO_LIB_DIR" ] && [ -d "$CRYPTO_LIB_DIR" ]; then
    echo "Found crypto library directory: $CRYPTO_LIB_DIR"
    cd "$CRYPTO_LIB_DIR"
    
    if [ -f "crypto.so" ]; then
        # Use otool to list linked libraries and grep for OpenSSL, then extract the first path
        openssl_lib=$(otool -L crypto.so 2>/dev/null | grep -E '/opt/(homebrew|local).*/libcrypto.*\.dylib' | awk '{print $1}' | head -1)
        
        # Check if the OpenSSL library was found
        if [ -n "$openssl_lib" ]; then
            echo "OpenSSL library found: $openssl_lib"
            
            if [ -f "$openssl_lib" ]; then
                echo "Copying OpenSSL library into release..."
                cp "$openssl_lib" .
                filename_with_ext=$(basename "$openssl_lib")
                
                echo "Updating crypto.so to use local OpenSSL library..."
                install_name_tool -change "$openssl_lib" "@loader_path/$filename_with_ext" crypto.so
                
                if [ -f "otp_test_engine.so" ]; then
                    echo "Updating otp_test_engine.so to use local OpenSSL library..."
                    install_name_tool -change "$openssl_lib" "@loader_path/$filename_with_ext" otp_test_engine.so
                fi
                
                echo "✓ SSL library dependencies fixed"
            else
                echo "Warning: OpenSSL library file not found at $openssl_lib"
                echo "  The app may not work on systems without this library installed"
            fi
        else
            echo "No Homebrew/MacPorts OpenSSL library dependency found"
            echo "  (This might be OK if using system OpenSSL or statically linked)"
        fi
    else
        echo "Warning: crypto.so not found in $CRYPTO_LIB_DIR"
    fi
else
    echo "Warning: Could not find crypto library directory"
    echo "  Expected path pattern: .../lib/crypto-*/priv/lib"
    echo "  (This might be OK if using a different Erlang/OTP version)"
fi

echo ""
echo "================================================"
echo "Step 2: Resolving symlinks in release directory..."
echo "================================================"

# Function to replace symlinks with actual content
replace_symlink() {
    local symlink="$1"
    local target=$(readlink "$symlink")

    # Resolve the absolute path of the symlink's target
    local absolute_target
    if [[ "$target" = /* ]]; then
        # Absolute path
        absolute_target="$target"
    else
        # Relative path - resolve from symlink's directory
        local symlink_dir
        symlink_dir="$(cd "$(dirname "$symlink")" && pwd)"
        absolute_target="$symlink_dir/$target"
    fi
    
    # Check if target exists
    if [ -e "$absolute_target" ]; then
        echo "  Replacing: $(basename "$symlink") -> $target"

        # Preserve permissions of the original symlink
        local permissions
        permissions=$(stat -f "%Lp" "$symlink" 2>/dev/null || echo "755")

        # Create a temporary location to copy the content
        local tmp_copy="${symlink}.tmp.$$"

        # Check if the symlink points to a file or directory
        if [ -d "$absolute_target" ]; then
            cp -R "$absolute_target" "$tmp_copy"
        else
            cp "$absolute_target" "$tmp_copy"
        fi

        # Remove the symlink and move the copied content to the original location
        rm "$symlink"
        mv "$tmp_copy" "$symlink"

        # Restore original permissions
        chmod "$permissions" "$symlink" 2>/dev/null || true

        echo "    ✓ Replaced with actual content"
    else
        # If the target doesn't exist, the symlink is broken
        echo "  Warning: Broken symlink - removing: $(basename "$symlink") -> $target"
        rm "$symlink"
    fi
}

# Navigate to the _build directory within the app bundle
cd "${RELEASE_APP_PATH}/Contents/Resources/_build"

echo "Searching for symlinks..."

# Find and count symlinks first
symlink_count=$(find . -type l 2>/dev/null | wc -l | tr -d ' ')

if [ "$symlink_count" -gt 0 ]; then
    echo "Found $symlink_count symlinks to process..."
    
    # Process each symlink
    while IFS= read -r symlink; do
        replace_symlink "$symlink"
    done < <(find . -type l 2>/dev/null)
    
    echo "✓ Successfully processed all symlinks"
else
    echo "No symlinks found (already processed or not needed)"
fi

echo ""
echo "================================================"
echo "Step 3: Verifying fixed release build..."
echo "================================================"

cd "${ROOT_DIR}/release"

# Test that the app still works after fixes
echo "Running health check..."
"${RELEASE_APP_DIR}/Contents/MacOS/Tau5" --check
if [ $? -eq 0 ]; then
    echo "✓ Tau5 health check passed"
else
    echo "ERROR: Tau5 health check failed after fixes!"
    exit 1
fi

# Also check tau5-node if it exists
if [ -f "${RELEASE_APP_DIR}/Contents/MacOS/tau5-node" ]; then
    "${RELEASE_APP_DIR}/Contents/MacOS/tau5-node" --check
    if [ $? -eq 0 ]; then
        echo "✓ tau5-node health check passed"
    else
        echo "Warning: tau5-node health check failed"
    fi
fi

echo ""
echo "========================================"
echo "Release build fixes completed!"
echo "========================================"
echo ""
echo "The app bundle at:"
echo "  ${ROOT_DIR}/release/${RELEASE_APP_DIR}"
echo ""
echo "is now ready for distribution with:"
echo "  ✓ OpenSSL libraries bundled (if needed)"
echo "  ✓ All symlinks resolved to actual files"
echo "  ✓ Health checks passed"
echo ""
echo "You can now distribute this app bundle to other macOS systems."
echo ""