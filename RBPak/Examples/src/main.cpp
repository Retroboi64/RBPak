#include "pak.h"
#include <iostream>
#include <vector>
#include <filesystem>
#include <cmath>

#ifndef M_PI
#define M_PI 3.141592
#endif

static void example_basic_usage() {
    std::cout << "\n=== Basic RBPak Example ===" << std::endl;

    RBPak pak;

    RBPak::print_config_info();

    std::vector<uint8_t> test_data = { 'H', 'e', 'l', 'l', 'o', ' ', 'R', 'B', 'K', '!' };
    std::vector<uint8_t> sprite_data;
    std::vector<uint8_t> sound_data;

    sprite_data.resize(32 * 32 * 4);
    for (size_t i = 0; i < sprite_data.size(); i += 4) {
        sprite_data[i] = static_cast<uint8_t>((i / 4) % 256);     // Red
        sprite_data[i + 1] = static_cast<uint8_t>((i / 8) % 256); // Green
        sprite_data[i + 2] = static_cast<uint8_t>((i / 16) % 256);// Blue
        sprite_data[i + 3] = 255;           // Alpha
    }

    // Generate some fake sound data
    sound_data.resize(44100); // 1 second of 8-bit mono
    for (size_t i = 0; i < sound_data.size(); i++) {
        double sample = std::sin(static_cast<double>(i) * 440.0 * 2.0 * 3.141592 / 44100.0) * 127.0 + 128.0;
        sound_data[i] = static_cast<uint8_t>(sample);
    }

    std::cout << "Adding files to RBK package..." << std::endl;

    pak.add_file("readme.txt", test_data);
    pak.add_file("sprites/player.rgba", sprite_data);
    pak.add_encrypted_file("sounds/jump.wav", sound_data); // This will be encrypted if enabled

    if (std::filesystem::exists("config.json")) {
        pak.add_file_from_disk("config.json", "config.json");
        std::cout << "Added config.json from disk" << std::endl;
    }

    if (pak.save_to_file("demo_assets.rbk")) {
        std::cout << "* Created demo_assets.rbk successfully!" << std::endl;
    }
    else {
        std::cout << "X Failed to create RBK file!" << std::endl;
        return;
    }

    std::cout << "\n--- Loading RBK Package ---" << std::endl;
    RBPak loader;
    if (loader.load_from_file("demo_assets.rbk")) {
        std::cout << "* Loaded demo_assets.rbk successfully!" << std::endl;

        auto readme = loader["readme.txt"];
        std::string readme_content(readme.begin(), readme.end());
        std::cout << "README content: " << readme_content << std::endl;

        auto sprite = loader["sprites/player.rgba"];
        std::cout << "Player sprite: " << sprite.size() << " bytes loaded" << std::endl;

        auto sound = loader["sounds/jump.wav"];
        std::cout << "Jump sound: " << sound.size() << " bytes loaded";
        if (sound.size() > 0) {
            std::cout << " (encrypted: " << (RBKConfig::ENCRYPTION_ENABLED ? "YES" : "NO") << ")";
        }
        std::cout << std::endl;

        auto files = loader.get_file_list();
        std::cout << "\nFiles in RBK package:" << std::endl;
        for (const auto& file : files) {
            auto data = loader[file];
            std::cout << "  * " << file << " (" << data.size() << " bytes)" << std::endl;
        }

        std::cout << "\nTotal files: " << loader.get_file_count() << std::endl;
    }
    else {
        std::cout << "X Failed to load RBK file!" << std::endl;
    }
}

static void example_engine_integration() {
    std::cout << "\n=== Engine Integration Example ===" << std::endl;

    class Project32AssetManager {
    private:
        RBPak sprites_, sounds_, levels_;

    public:
        bool load_game_assets() {
            std::cout << "Project32: Loading game assets..." << std::endl;

            bool success = true;

            RBPak demo_pack;
            std::vector<uint8_t> demo_sprite = { 0x89, 0x50, 0x4E, 0x47 };
            std::vector<uint8_t> demo_sound = { 0x52, 0x49, 0x46, 0x46 };

            demo_pack.add_file("sprites/hero.png", demo_sprite);
            demo_pack.add_encrypted_file("sounds/bgm.wav", demo_sound);
            demo_pack.save_to_file("game_assets.rbk");

            success = success && sprites_.load_from_file("game_assets.rbk");

            if (success) {
                std::cout << "* All asset packs loaded successfully!" << std::endl;
            }

            return success;
        }

        std::vector<uint8_t> get_sprite(const std::string& name) {
            return sprites_["sprites/" + name];
        }

        std::vector<uint8_t> get_sound(const std::string& name) {
            return sprites_["sounds/" + name];
        }

        void print_stats() {
            std::cout << "Asset Statistics:" << std::endl;
            std::cout << "  Sprites loaded: " << sprites_.get_file_count() << std::endl;
        }
    };

    Project32AssetManager assets;
    if (assets.load_game_assets()) {
        auto hero_sprite = assets.get_sprite("hero.png");
        auto bgm = assets.get_sound("bgm.wav");

        std::cout << "Loaded hero sprite: " << hero_sprite.size() << " bytes" << std::endl;
        std::cout << "Loaded background music: " << bgm.size() << " bytes" << std::endl;

        assets.print_stats();

        std::cout << "* Project32 integration demo complete!" << std::endl;
    }
}

static void print_help() {}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        }
        else if (arg == "--version") {
            return 0;
        }
        else if (arg == "--config") {
            RBPak::print_config_info();
            return 0;
        }
    }

    std::cout << "Asset packaging system" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    try {
        example_basic_usage();
        example_engine_integration();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}