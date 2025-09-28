#!/usr/bin/env python3

import json
import os
import sys
from pathlib import Path
from datetime import datetime

def create_banner():
    return """
██████╗ ███████╗████████╗██████╗  ██████╗ ██████╗  ██████╗ ██╗ ██████╗ ██╗  ██╗
██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██╔══██╗██╔═══██╗██║██╔════╝ ██║  ██║
██████╔╝█████╗     ██║   ██████╔╝██║   ██║██████╔╝██║   ██║██║███████╗ ███████║
██╔══██╗██╔══╝     ██║   ██╔══██╗██║   ██║██╔══██╗██║   ██║██║██╔═══██╗╚════██║
██║  ██║███████╗   ██║   ██║  ██║╚██████╔╝██████╔╝╚██████╔╝██║╚██████╔╝     ██║
╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚═════╝  ╚═════╝ ╚═╝ ╚═════╝      ╚═╝
                          Asset Configuration Generator v2.0
                               For Project32 Game Engine
"""

def load_config():
    """Load configuration from config.json or create default"""
    config_file = Path("config.json")
    
    if not config_file.exists():
        default_config = {
            "project": {
                "name": "Project32Game",
                "version": "1.0.0",
                "author": "Retroboi64"
            },
            "encryption": {
                "enabled": False,
                "method": "XOR",
                "key": "DefaultKey2024_Project32"
            },
            "compression": {
                "level": 6,
                "algorithm": "zlib"
            },
            "security": {
                "obfuscate_filenames": False,
                "verify_checksums": True,
                "signature_key": "Project32Engine_v1.0"
            },
            "build": {
                "debug_output": True,
                "progress_indicators": True
            }
        }
        
        with open(config_file, 'w') as f:
            json.dump(default_config, f, indent=2)
        print(f"✓ Created default {config_file}")
        print("  Edit this file to customize your RBK settings!")
    
    try:
        with open(config_file, 'r') as f:
            config = json.load(f)
            print(f"✓ Loaded configuration from {config_file}")
            return config
    except json.JSONDecodeError as e:
        print(f"✗ JSON Error in {config_file}: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"✗ Error loading {config_file}: {e}")
        sys.exit(1)

def validate_config(config):
    """Validate configuration values"""
    errors = []
    
    encryption = config.get("encryption", {})
    if encryption.get("enabled", False):
        method = encryption.get("method", "")
        if method not in ["XOR", "AES"]:
            errors.append(f"Unsupported encryption method: {method}")
        
        key = encryption.get("key", "")
        if len(key) < 8:
            errors.append("Encryption key must be at least 8 characters")
    
    compression = config.get("compression", {})
    level = compression.get("level", 6)
    if not isinstance(level, int) or level < 0 or level > 9:
        errors.append(f"Compression level must be 0-9, got: {level}")
    
    project = config.get("project", {})
    if not project.get("name", "").strip():
        errors.append("Project name cannot be empty")
    
    if errors:
        print("✗ Configuration validation failed:")
        for error in errors:
            print(f"  - {error}")
        return False
    
    print("✓ Configuration validated successfully")
    return True

def generate_header(config):
    """Generate pak_config.h from configuration"""
    
    project = config.get("project", {})
    encryption = config.get("encryption", {})
    compression = config.get("compression", {})
    security = config.get("security", {})
    build = config.get("build", {})
    
    project_name = project.get("name", "Project32Game")
    project_version = project.get("version", "1.0.0")
    project_author = project.get("author", "Retroboi64")
    
    encryption_enabled = str(encryption.get("enabled", False)).lower()
    encryption_method = encryption.get("method", "XOR")
    encryption_key = encryption.get("key", "DefaultKey2024_Project32")
    
    compression_level = compression.get("level", 6)
    compression_algorithm = compression.get("algorithm", "zlib")
    
    obfuscate_filenames = str(security.get("obfuscate_filenames", False)).lower()
    verify_checksums = str(security.get("verify_checksums", True)).lower()
    signature_key = security.get("signature_key", "Project32Engine_v1.0")
    
    debug_output = str(build.get("debug_output", True)).lower()
    progress_indicators = str(build.get("progress_indicators", True)).lower()
    
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    header_content = f'''#pragma once

// ============================================================================
// RetroBoi64 Configuration Header (Auto-Generated)
// ============================================================================
// Generated: {timestamp}
// Project: {project_name} v{project_version}
// Author: {project_author}
// 
// WARNING: DO NOT EDIT THIS FILE MANUALLY!
// This file is auto-generated from config.json by generate_config.py
// ============================================================================

#define RBK_USE_CONFIG 1

namespace RBKConfig {{
    // Project Information
    constexpr const char* PROJECT_NAME = "{project_name}";
    constexpr const char* PROJECT_VERSION = "{project_version}";
    constexpr const char* PROJECT_AUTHOR = "{project_author}";
    
    // Encryption Settings
    constexpr bool ENCRYPTION_ENABLED = {encryption_enabled};
    constexpr const char* ENCRYPTION_KEY = "{encryption_key}";
    constexpr const char* ENCRYPTION_METHOD = "{encryption_method}";
    
    // Compression Settings
    constexpr int COMPRESSION_LEVEL = {compression_level};
    constexpr const char* COMPRESSION_ALGORITHM = "{compression_algorithm}";
    
    // Security Settings
    constexpr bool OBFUSCATE_FILENAMES = {obfuscate_filenames};
    constexpr bool VERIFY_CHECKSUMS = {verify_checksums};
    constexpr const char* SIGNATURE_KEY = "{signature_key}";
    
    // Build Settings
    constexpr bool DEBUG_OUTPUT = {debug_output};
    constexpr bool PROGRESS_INDICATORS = {progress_indicators};
    
    // Version Information
    constexpr const char* RBK_VERSION = "2.0.0";
    constexpr const char* ENGINE_NAME = "Project32";
    constexpr const char* BUILD_TIMESTAMP = "{timestamp}";
}}

// Configuration Summary (for reference):
/*
╔══════════════════════════════════════════════════════════════════════════════╗
║                         RetroBoi64 Build Configuration                        ║
╠══════════════════════════════════════════════════════════════════════════════╣
║ Project: {project_name:<62} ║
║ Version: {project_version:<62} ║
║ Author:  {project_author:<62} ║
║                                                                              ║
║ Security Features:                                                           ║
║   • Encryption: {("ENABLED (" + encryption_method + ")"):<54} ║
║   • Filename Obfuscation: {("ON" if obfuscate_filenames == "true" else "OFF"):<44} ║
║   • Checksum Verification: {("ON" if verify_checksums == "true" else "OFF"):<43} ║
║                                                                              ║
║ Performance Settings:                                                        ║
║   • Compression Level: {compression_level}/9{"":<49} ║
║   • Algorithm: {compression_algorithm:<58} ║
║                                                                              ║
║ Build Options:                                                               ║
║   • Debug Output: {("ON" if debug_output == "true" else "OFF"):<55} ║
║   • Progress Indicators: {("ON" if progress_indicators == "true" else "OFF"):<48} ║
║                                                                              ║
║ Generated: {timestamp:<60} ║
╚══════════════════════════════════════════════════════════════════════════════╝
*/
'''
    
    return header_content

def create_build_batch():
    """Create build.bat for Windows users"""
    batch_content = '''@echo off
echo.
echo ===================================
echo   RetroBoi64 Build Script
echo   Project32 Engine Asset System  
echo ===================================
echo.

echo [1/3] Generating configuration...
python generate_config.py
if errorlevel 1 (
    echo ERROR: Config generation failed!
    pause
    exit /b 1
)

echo.
echo [2/3] Building RetroBoi64 library...
if not exist "build" mkdir build
cd build
cmake .. -DRBK_USE_CONFIG=ON
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo [3/3] Build complete!
echo.
echo Your RetroBoi64 library is ready for Project32!
echo Check the 'build' directory for compiled files.
echo.
pause
'''
    
    with open("build.bat", 'w') as f:
        f.write(batch_content)
    print("✓ Created build.bat for easy Windows building")

def main():
    os.system('cls' if os.name == 'nt' else 'clear')
    print(create_banner())
    
    print("Initializing RetroBoi64 configuration system...")
    print("=" * 60)
    
    config = load_config()
    if not validate_config(config):
        sys.exit(1)
    
    header_content = generate_header(config)
    
    include_dir = Path("include")
    include_dir.mkdir(exist_ok=True)
    
    header_file = include_dir / "pak_config.h"
    try:
        with open(header_file, 'w', encoding='utf-8') as f:
            f.write(header_content)
        print(f"✓ Generated: {header_file}")
    except Exception as e:
        print(f"✗ Error writing {header_file}: {e}")
        sys.exit(1)
    
    create_build_batch()
    
    print("\n" + "=" * 60)
    print("CONFIGURATION SUMMARY")
    print("=" * 60)
    
    project = config.get("project", {})
    encryption = config.get("encryption", {})
    compression = config.get("compression", {})
    security = config.get("security", {})
    
    print(f"Project: {project.get('name', 'Unknown')} v{project.get('version', '1.0')}")
    print(f"Author: {project.get('author', 'Unknown')}")
    print()
    
    print("Security Features:")
    enc_status = "ON" if encryption.get('enabled', False) else "OFF"
    print(f"  Encryption: {enc_status}")
    if encryption.get('enabled', False):
        print(f"    Method: {encryption.get('method', 'XOR')}")
        key_len = len(encryption.get('key', ''))
        print(f"    Key Length: {key_len} characters")
    
    obf_status = "ON" if security.get('obfuscate_filenames', False) else "OFF"
    print(f"  Filename Obfuscation: {obf_status}")
    
    chk_status = "ON" if security.get('verify_checksums', True) else "OFF"
    print(f"  Checksum Verification: {chk_status}")
    
    print()
    print("Performance:")
    comp_level = compression.get('level', 6)
    comp_desc = ["None", "Fastest", "Fast", "Fast", "Normal", "Normal", 
                 "Good", "Good", "Better", "Best"][comp_level]
    print(f"  Compression: Level {comp_level}/9 ({comp_desc})")
    
    print("\n" + "=" * 60)
    print("RetroBoi64 is ready for Project32 Engine!")
    print()
    print("Next steps:")
    print("  1. Run 'build.bat' (Windows) or build with your IDE")
    print("  2. Link RetroBoi64 library to your Project32 game")
    print("  3. Start packing your game assets!")
    print()
    print("Happy retro game development!")
    print("=" * 60)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nBuild cancelled by user.")
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")
        sys.exit(1)