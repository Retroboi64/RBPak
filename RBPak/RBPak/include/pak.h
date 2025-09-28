#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <memory>
#include <cstring>

struct RBKEntry {
    std::string name;
    uint32_t offset;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t crc32;
    std::vector<uint8_t> data;
    bool is_loaded;

    RBKEntry() : offset(0), compressed_size(0), uncompressed_size(0), crc32(0), is_loaded(false) {}
};

class PAK {
private:
    static const uint32_t RBK_SIGNATURE = 0x014B4252; // "RBK\x01"
    std::unordered_map<std::string, std::unique_ptr<RBKEntry>> entries_;
    std::string file_path_;
    std::ifstream file_stream_;

    uint32_t calculate_crc32(const uint8_t* data, size_t length);
    bool compress_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output);
    bool decompress_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output, size_t expected_size);

public:
    PAK();
    ~PAK();

    bool add_file(const std::string& name, const std::vector<uint8_t>& data);
    bool add_file_from_disk(const std::string& name, const std::string& file_path);
    bool save_to_file(const std::string& file_path);

    bool load_from_file(const std::string& file_path);
    bool has_file(const std::string& name) const;
    std::vector<uint8_t> get_file(const std::string& name);
    std::vector<std::string> get_file_list() const;

    size_t get_file_count() const;
    void clear();

    std::vector<uint8_t> operator[](const std::string& name);
};