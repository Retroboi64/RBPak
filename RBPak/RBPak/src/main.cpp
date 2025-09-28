#include "../include/pak.h"
#include <iostream>
#include <vector>

int main() {
    PAK pak;

    std::vector<uint8_t> test_data = { 'H', 'e', 'l', 'l', 'o', ' ', 'R', 'B', 'K', '!' };

    pak.add_file("test.txt", test_data);

    if (pak.save_to_file("test.rbk")) {
        std::cout << "Created test.rbk successfully!\n";
    }

    PAK loader;
    if (loader.load_from_file("test.rbk")) {
        std::cout << "Loaded test.rbk successfully!\n";

        auto data = loader["test.txt"];
        std::string content(data.begin(), data.end());
        std::cout << "Content: " << content << "\n";

        auto files = loader.get_file_list();
        std::cout << "Files in RBK:\n";
        for (const auto& file : files) {
            std::cout << "  - " << file << "\n";
        }
    }

    return 0;
}