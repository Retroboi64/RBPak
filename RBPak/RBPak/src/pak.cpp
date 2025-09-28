#include "../include/pak.h"
#include <zlib.h>
#include <iostream>

PAK::PAK() {
}

PAK::~PAK() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

uint32_t PAK::calculate_crc32(const uint8_t* data, size_t length) {
    return crc32(0L, data, static_cast<uInt>(length));
}

bool PAK::compress_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output) {
    uLongf compressed_size = compressBound(static_cast<uLong>(input_size));
    output.resize(compressed_size);

    int result = compress(output.data(), &compressed_size, input, static_cast<uLong>(input_size));
    if (result != Z_OK) {
        std::cerr << "Compression failed with error: " << result << std::endl;
        return false;
    }

    output.resize(compressed_size);
    return true;
}

bool PAK::decompress_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output, size_t expected_size) {
    output.resize(expected_size);
    uLongf uncompressed_size = static_cast<uLongf>(expected_size);

    int result = uncompress(output.data(), &uncompressed_size, input, static_cast<uLong>(input_size));
    if (result != Z_OK) {
        std::cerr << "Decompression failed with error: " << result << std::endl;
        return false;
    }

    output.resize(uncompressed_size);
    return true;
}

bool PAK::add_file(const std::string& name, const std::vector<uint8_t>& data) {
    if (name.empty() || data.empty()) {
        return false;
    }

    auto entry = std::make_unique<RBKEntry>();
    entry->name = name;
    entry->data = data;
    entry->uncompressed_size = static_cast<uint32_t>(data.size());
    entry->crc32 = calculate_crc32(data.data(), data.size());
    entry->is_loaded = true;

    entries_[name] = std::move(entry);
    return true;
}

bool PAK::add_file_from_disk(const std::string& name, const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << file_path << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(file_size);
    file.read(reinterpret_cast<char*>(data.data()), file_size);
    file.close();

    return add_file(name, data);
}

bool PAK::save_to_file(const std::string& file_path) {
    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not create file: " << file_path << std::endl;
        return false;
    }

    uint32_t signature = RBK_SIGNATURE;
    uint32_t entry_count = static_cast<uint32_t>(entries_.size());
    uint32_t directory_offset = 0;

    file.write(reinterpret_cast<const char*>(&signature), 4);
    file.write(reinterpret_cast<const char*>(&entry_count), 4);
    file.write(reinterpret_cast<const char*>(&directory_offset), 4);

    std::vector<std::pair<std::string, RBKEntry*>> sorted_entries;
    for (auto& pair : entries_) {
        sorted_entries.push_back({ pair.first, pair.second.get() });
    }

    uint32_t current_offset = 12;
    for (auto& pair : sorted_entries) {
        RBKEntry* entry = pair.second;

        std::vector<uint8_t> compressed_data;
        if (!compress_data(entry->data.data(), entry->data.size(), compressed_data)) {
            std::cerr << "Failed to compress: " << entry->name << std::endl;
            return false;
        }

        entry->offset = current_offset;
        entry->compressed_size = static_cast<uint32_t>(compressed_data.size());

        file.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
        current_offset += static_cast<uint32_t>(compressed_data.size());
    }

    directory_offset = current_offset;
    for (const auto& pair : sorted_entries) {
        const RBKEntry* entry = pair.second;

        uint16_t name_length = static_cast<uint16_t>(entry->name.length());
        file.write(reinterpret_cast<const char*>(&name_length), 2);
        file.write(entry->name.c_str(), name_length);
        file.write(reinterpret_cast<const char*>(&entry->offset), 4);
        file.write(reinterpret_cast<const char*>(&entry->compressed_size), 4);
        file.write(reinterpret_cast<const char*>(&entry->uncompressed_size), 4);
        file.write(reinterpret_cast<const char*>(&entry->crc32), 4);
    }

    file.seekp(8);
    file.write(reinterpret_cast<const char*>(&directory_offset), 4);

    file.close();
    std::cout << "Successfully saved " << entries_.size() << " files to " << file_path << std::endl;
    return true;
}

bool PAK::load_from_file(const std::string& file_path) {
    clear();

    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    file_stream_.open(file_path, std::ios::binary);
    if (!file_stream_.is_open()) {
        std::cerr << "Could not open RBK file: " << file_path << std::endl;
        return false;
    }

    file_path_ = file_path;

    uint32_t signature, entry_count, directory_offset;
    file_stream_.read(reinterpret_cast<char*>(&signature), 4);
    file_stream_.read(reinterpret_cast<char*>(&entry_count), 4);
    file_stream_.read(reinterpret_cast<char*>(&directory_offset), 4);

    if (signature != RBK_SIGNATURE) {
        std::cerr << "Invalid RBK file signature" << std::endl;
        return false;
    }

    file_stream_.seekg(directory_offset);
    for (uint32_t i = 0; i < entry_count; i++) {
        auto entry = std::make_unique<RBKEntry>();

        uint16_t name_length;
        file_stream_.read(reinterpret_cast<char*>(&name_length), 2);

        entry->name.resize(name_length);
        file_stream_.read(&entry->name[0], name_length);

        file_stream_.read(reinterpret_cast<char*>(&entry->offset), 4);
        file_stream_.read(reinterpret_cast<char*>(&entry->compressed_size), 4);
        file_stream_.read(reinterpret_cast<char*>(&entry->uncompressed_size), 4);
        file_stream_.read(reinterpret_cast<char*>(&entry->crc32), 4);

        entry->is_loaded = false;
        entries_[entry->name] = std::move(entry);
    }

    std::cout << "Successfully loaded " << entry_count << " files from " << file_path << std::endl;
    return true;
}

bool PAK::has_file(const std::string& name) const {
    return entries_.find(name) != entries_.end();
}

std::vector<uint8_t> PAK::get_file(const std::string& name) {
    auto it = entries_.find(name);
    if (it == entries_.end()) {
        return std::vector<uint8_t>();
    }

    RBKEntry* entry = it->second.get();

    if (!entry->is_loaded && file_stream_.is_open()) {
        std::vector<uint8_t> compressed_data(entry->compressed_size);
        file_stream_.seekg(entry->offset);
        file_stream_.read(reinterpret_cast<char*>(compressed_data.data()), entry->compressed_size);

        if (!decompress_data(compressed_data.data(), compressed_data.size(),
            entry->data, entry->uncompressed_size)) {
            std::cerr << "Failed to decompress: " << entry->name << std::endl;
            return std::vector<uint8_t>();
        }

        uint32_t calculated_crc = calculate_crc32(entry->data.data(), entry->data.size());
        if (calculated_crc != entry->crc32) {
            std::cerr << "CRC32 mismatch for: " << entry->name << std::endl;
            return std::vector<uint8_t>();
        }

        entry->is_loaded = true;
    }

    return entry->data;
}

std::vector<std::string> PAK::get_file_list() const {
    std::vector<std::string> file_list;
    file_list.reserve(entries_.size());

    for (const auto& pair : entries_) {
        file_list.push_back(pair.first);
    }

    return file_list;
}

size_t PAK::get_file_count() const {
    return entries_.size();
}

void PAK::clear() {
    entries_.clear();
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    file_path_.clear();
}

std::vector<uint8_t> PAK::operator[](const std::string& name) {
    return get_file(name);
}