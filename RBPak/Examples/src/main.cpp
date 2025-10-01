/*
 * RBPak - Example Usage
 * Demonstrates how to use the RBPak library
 * Requires C++20 compiler
 */

#include "pak.h"
#include <iostream>
#include <fstream>

using namespace rbpak;

// Progress callback example
void ProgressReporter(size_t current, size_t total, std::string_view filename) {
    std::cout << "[" << current << "/" << total << "] Processing: " << filename << std::endl;
}

// Example 1: Basic usage - Create a package
void Example_CreatePackage() {
    std::cout << "\n=== Example 1: Create Package ===" << std::endl;

    // Create package with default settings
    Package pak;

    // Add files from memory
    std::string text = "Hello, RBPak!";
    ByteArray data(text.begin(), text.end());

    if (auto result = pak.Add("hello.txt", data); result) {
        std::cout << "Added hello.txt" << std::endl;
    }
    else {
        std::cout << "Error: " << result.message << std::endl;
    }

    // Add more data
    ByteArray binary = { 0x89, 0x50, 0x4E, 0x47 }; // PNG header
    pak.Add("image.png", binary);

    // Save package
    if (auto result = pak.Save("example.pak"); result) {
        std::cout << "Package saved successfully!" << std::endl;
        pak.PrintStatistics();
    }
    else {
        std::cout << "Save failed: " << result.message << std::endl;
    }
}

// Example 2: Secure package with encryption
void Example_SecurePackage() {
    std::cout << "\n=== Example 2: Secure Package ===" << std::endl;

    // Create secure package
    PackageConfig config = PackageConfig::Secure("MySecretKey123");
    config.compression = CompressionLevel::Best;

    Package pak(config);

    // Add sensitive data
    std::string secret = "This is encrypted data!";
    pak.Add("secret.txt", ByteArray(secret.begin(), secret.end()));

    // Save with encryption
    if (auto result = pak.Save("secure.pak"); result) {
        std::cout << "Secure package created!" << std::endl;
        pak.PrintStatistics();
    }
}

// Example 3: Load and extract files
void Example_LoadAndExtract() {
    std::cout << "\n=== Example 3: Load and Extract ===" << std::endl;

    Package pak;

    // Load existing package
    if (auto result = pak.Load("example.pak"); !result) {
        std::cout << "Failed to load: " << result.message << std::endl;
        return;
    }

    std::cout << "Package loaded successfully!" << std::endl;
    std::cout << "Files in package:" << std::endl;

    // List all files
    for (const auto& filename : pak.List()) {
        std::cout << "  - " << filename;

        // Get file info
        if (auto info = pak.GetFileInfo(filename)) {
            std::cout << " (" << pak_utils::FormatSize(info->uncompressed_size) << ")";
        }
        std::cout << std::endl;
    }

    // Extract specific file
    if (auto data = pak.Get("hello.txt")) {
        std::string content(data->begin(), data->end());
        std::cout << "\nContent of hello.txt: " << content << std::endl;
    }

    // Extract to file
    if (auto result = pak.Extract("hello.txt", "extracted_hello.txt"); result) {
        std::cout << "Extracted to extracted_hello.txt" << std::endl;
    }

    // Extract all files
    if (auto result = pak.ExtractAll("extracted/"); result) {
        std::cout << "All files extracted to extracted/" << std::endl;
    }
}

// Example 4: Working with directories
void Example_AddDirectory() {
    std::cout << "\n=== Example 4: Add Directory ===" << std::endl;

    // Create some test files first
    std::ofstream("test_data/file1.txt") << "File 1 content";
    std::ofstream("test_data/file2.txt") << "File 2 content";
    std::ofstream("test_data/subdir/file3.txt") << "File 3 content";

    Package pak;

    // Add entire directory recursively with progress
    if (auto result = pak.AddDirectory("test_data", true, ProgressReporter); result) {
        std::cout << "\nDirectory added successfully!" << std::endl;
        pak.PrintStatistics();
        pak.Save("directory.pak");
    }
    else {
        std::cout << "Failed: " << result.message << std::endl;
    }
}

// Example 5: Advanced usage - Multiple files
void Example_MultipleFiles() {
    std::cout << "\n=== Example 5: Add Multiple Files ===" << std::endl;

    Package pak;

    // Prepare multiple files
    std::vector<std::pair<std::string, ByteArray>> files;

    for (int i = 0; i < 5; i++) {
        std::string name = "file" + std::to_string(i) + ".txt";
        std::string content = "Content of file " + std::to_string(i);
        files.emplace_back(name, ByteArray(content.begin(), content.end()));
    }

    // Add all at once with progress
    if (auto result = pak.AddMultiple(files, ProgressReporter); result) {
        std::cout << "\nAll files added!" << std::endl;
        pak.Save("multiple.pak");
    }
}

// Example 6: File management
void Example_FileManagement() {
    std::cout << "\n=== Example 6: File Management ===" << std::endl;

    Package pak;
    pak.Load("example.pak");

    // Check if file exists
    if (pak.Has("hello.txt")) {
        std::cout << "hello.txt exists in package" << std::endl;
    }

    // Get detailed file information
    for (const auto& info : pak.ListDetailed()) {
        std::cout << "\nFile: " << info.name << std::endl;
        std::cout << "  Stored as: " << info.stored_name << std::endl;
        std::cout << "  Size: " << pak_utils::FormatSize(info.uncompressed_size) << std::endl;
        std::cout << "  Compressed: " << pak_utils::FormatSize(info.compressed_size) << std::endl;
        std::cout << "  Ratio: " << (info.GetCompressionRatio() * 100.0f) << "%" << std::endl;
        std::cout << "  Encrypted: " << (info.is_encrypted ? "Yes" : "No") << std::endl;
        std::cout << "  CRC32: 0x" << std::hex << info.crc32 << std::dec << std::endl;
    }

    // Remove a file
    if (pak.Remove("hello.txt")) {
        std::cout << "\nRemoved hello.txt from package" << std::endl;
    }

    // Save modified package
    pak.Save("modified.pak");
}

// Example 7: Fast loading configuration
void Example_FastLoading() {
    std::cout << "\n=== Example 7: Fast Loading ===" << std::endl;

    // Use fast loading configuration
    PackageConfig config = PackageConfig::FastLoad();
    Package pak(config);

    pak.Add("data.bin", ByteArray(1024 * 1024, 0xFF)); // 1MB of data
    pak.Save("fast.pak");

    std::cout << "Package optimized for fast loading!" << std::endl;
}

// Example 8: Error handling
void Example_ErrorHandling() {
    std::cout << "\n=== Example 8: Error Handling ===" << std::endl;

    Package pak;

    // Try to load non-existent package
    if (auto result = pak.Load("nonexistent.pak"); !result) {
        std::cout << "Expected error: " << result.message << std::endl;
        std::cout << "Error code: " << pak_utils::GetErrorMessage(result.error) << std::endl;
    }

    // Try to add invalid data
    if (auto result = pak.Add("", ByteArray{}); !result) {
        std::cout << "Expected error: " << result.message << std::endl;
    }

    // Check last error
    if (pak.GetLastError() != PackageError::None) {
        std::cout << "Last error: " << pak_utils::GetErrorMessage(pak.GetLastError()) << std::endl;
    }
}

// Example 9: Cache management
void Example_CacheManagement() {
    std::cout << "\n=== Example 9: Cache Management ===" << std::endl;

    PackageConfig config;
    config.lazy_load = true;
    config.max_cache_size = 10 * 1024 * 1024; // 10MB cache

    Package pak(config);
    pak.Load("example.pak");

    // Access files (will be cached)
    auto data1 = pak["hello.txt"];
    auto data2 = pak["image.png"];

    std::cout << "Cache size: " << pak_utils::FormatSize(pak.GetCacheSize()) << std::endl;

    // Clear cache
    pak.ClearCache();
    std::cout << "Cache cleared" << std::endl;
}

// Example 10: Operator overload usage
void Example_OperatorOverload() {
    std::cout << "\n=== Example 10: Operator Overload ===" << std::endl;

    Package pak;
    pak.Load("example.pak");

    // Use [] operator to access files
    if (auto data = pak["hello.txt"]) {
        std::string content(data->begin(), data->end());
        std::cout << "Using [] operator: " << content << std::endl;
    }
}

int main() {
    std::cout << "==================================" << std::endl;
    std::cout << "    RBPak Library Examples" << std::endl;
    std::cout << "    Requires C++20 compiler" << std::endl;
    std::cout << "==================================" << std::endl;

    try {
        Example_CreatePackage();
        Example_LoadAndExtract();
        Example_SecurePackage();
        Example_MultipleFiles();
        Example_FileManagement();
        Example_FastLoading();
        Example_ErrorHandling();
        Example_CacheManagement();
        Example_OperatorOverload();

        // Uncomment if you have a test_data directory
        // Example_AddDirectory();

        std::cout << "\n==================================" << std::endl;
        std::cout << "All examples completed!" << std::endl;
        std::cout << "==================================" << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}