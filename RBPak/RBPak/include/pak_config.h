#pragma once

namespace RBKConfig {
    constexpr const char* PROJECT_NAME = "Project32";
    constexpr const char* PROJECT_VERSION = "1.0.0";
    constexpr const char* PROJECT_AUTHOR = "Retroboi64";

    constexpr bool ENCRYPTION_ENABLED = false;
    constexpr const char* ENCRYPTION_KEY = "DefaultKey2024_Project32";
    constexpr const char* ENCRYPTION_METHOD = "XOR";

    constexpr int COMPRESSION_LEVEL = 6;
    constexpr const char* COMPRESSION_ALGORITHM = "zlib";

    constexpr bool OBFUSCATE_FILENAMES = false;
    constexpr bool VERIFY_CHECKSUMS = true;
    constexpr const char* SIGNATURE_KEY = "Project32Engine_v1.0";

    constexpr bool DEBUG_OUTPUT = true;
    constexpr bool PROGRESS_INDICATORS = true;

    constexpr const char* RBK_VERSION = "2.0.0";
    constexpr const char* ENGINE_NAME = "Project32";
    constexpr const char* BUILD_TIMESTAMP = "Default Build";
}