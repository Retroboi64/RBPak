#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <memory>
#include <cstring>
#include <array>

#include "pak_config.h"

struct RBKEntry {
    std::string name;
    std::string original_name;
    uint32_t offset;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t crc32;
    uint32_t name_hash;
    std::vector<uint8_t> data;
    bool is_loaded;
    bool is_encrypted;

    RBKEntry() : offset(0), compressed_size(0), uncompressed_size(0),
        crc32(0), name_hash(0), is_loaded(false), is_encrypted(false) {
    }
};

class SimpleXORCipher {
private:
    std::vector<uint8_t> key_;

public:
    SimpleXORCipher(const std::vector<uint8_t>& key) : key_(key) {}

    void encrypt_decrypt(uint8_t* data, size_t length) {
        if (key_.empty()) return;

        for (size_t i = 0; i < length; i++) {
            data[i] ^= key_[i % key_.size()];
        }
    }
};

class PAK {
private:
    static const uint32_t RBK_SIGNATURE = 0x024B5252;
    std::unordered_map<std::string, std::unique_ptr<RBKEntry>> entries_;
    std::unordered_map<uint32_t, std::string> hash_to_name_;
    std::string file_path_;
    std::ifstream file_stream_;
    std::array<uint8_t, 32> encryption_key_;
    bool encryption_initialized_;

    void initialize_encryption();
    std::array<uint8_t, 32> derive_key_from_string(const std::string& key_string);
    bool encrypt_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output);
    bool decrypt_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output);

    bool compress_data_level(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output, int level);

    uint32_t calculate_crc32(const uint8_t* data, size_t length);
    bool compress_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output);
    bool decompress_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output, size_t expected_size);
    uint32_t hash_filename(const std::string& filename);
    std::string obfuscate_filename(const std::string& original);

public:
    PAK();
    ~PAK();

    bool add_file(const std::string& name, const std::vector<uint8_t>& data);
    bool add_file_from_disk(const std::string& name, const std::string& file_path);
    bool add_encrypted_file(const std::string& name, const std::vector<uint8_t>& data);
    bool save_to_file(const std::string& file_path);

    bool load_from_file(const std::string& file_path);
    bool has_file(const std::string& name) const;
    std::vector<uint8_t> get_file(const std::string& name);
    std::vector<std::string> get_file_list() const;

    size_t get_file_count() const;
    void clear();

    std::vector<uint8_t> operator[](const std::string& name);

    static void print_config_info();

};