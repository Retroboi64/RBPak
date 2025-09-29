#include "pak.h"
#include "pak_config.h"
#include <zlib.h>
#include <iostream>


RBPak::RBPak() : encryption_initialized_(false) {
    initialize_encryption();
}

RBPak::~RBPak() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void RBPak::initialize_encryption() {
    if (RBKConfig::ENCRYPTION_ENABLED) {
        encryption_key_ = derive_key_from_string(RBKConfig::ENCRYPTION_KEY);
        encryption_initialized_ = true;
    }
}

std::array<uint8_t, 32> RBPak::derive_key_from_string(const std::string& key_string) {
    std::array<uint8_t, 32> key = {};

    std::string current = key_string + RBKConfig::SIGNATURE_KEY;

    for (int i = 0; i < 32; i++) {
        uint32_t hash = 0;
        for (char c : current) {
            hash = hash * 31 + static_cast<uint32_t>(c);
        }
        key[i] = static_cast<uint8_t>(hash & 0xFF);
        current += std::to_string(hash);
    }

    return key;
}

bool RBPak::encrypt_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output) {
    if (!RBKConfig::ENCRYPTION_ENABLED) {
        return true;
    }
    if (!encryption_initialized_) {
        return true;
    }

    output.assign(input, input + input_size);

    if (std::string(RBKConfig::ENCRYPTION_METHOD) == "XOR") {
        std::vector<uint8_t> key(encryption_key_.begin(), encryption_key_.end());
        SimpleXORCipher cipher(key);
        cipher.encrypt_decrypt(output.data(), output.size());
        return true;
    }

    return false;
}

bool RBPak::decrypt_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output) {


    output.assign(input, input + input_size);

    if (std::string(RBKConfig::ENCRYPTION_METHOD) == "XOR") {
        std::vector<uint8_t> key(encryption_key_.begin(), encryption_key_.end());
        SimpleXORCipher cipher(key);
        cipher.encrypt_decrypt(output.data(), output.size()); // XOR is symmetric
        return true;
    }

    return false;
}

uint32_t RBPak::calculate_crc32(const uint8_t* data, size_t length) {
    return crc32(0L, data, static_cast<uInt>(length));
}

bool RBPak::compress_data_level(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output, int level) {
    uLongf compressed_size = compressBound(static_cast<uLong>(input_size));
    output.resize(compressed_size);

    int result = compress2(output.data(), &compressed_size, input, static_cast<uLong>(input_size), level);
    if (result != Z_OK) {
        return false;
    }

    output.resize(compressed_size);
    return true;
}

bool RBPak::compress_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output) {
    return compress_data_level(input, input_size, output, RBKConfig::COMPRESSION_LEVEL);
}

bool RBPak::decompress_data(const uint8_t* input, size_t input_size, std::vector<uint8_t>& output, size_t expected_size) {
    output.resize(expected_size);
    uLongf uncompressed_size = static_cast<uLongf>(expected_size);

    int result = uncompress(output.data(), &uncompressed_size, input, static_cast<uLong>(input_size));
    if (result != Z_OK) {
        return false;
    }

    output.resize(uncompressed_size);
    return true;
}

uint32_t RBPak::hash_filename(const std::string& filename) {
    uint32_t hash = 0;
    for (char c : filename) {
        hash = hash * 31 + static_cast<uint32_t>(c);
    }
    return hash;
}

std::string RBPak::obfuscate_filename(const std::string& original) {
    if (!RBKConfig::OBFUSCATE_FILENAMES) {
        return original;
    }

    uint32_t hash = hash_filename(original + RBKConfig::SIGNATURE_KEY);
    return "f_" + std::to_string(hash) + ".dat";
}

bool RBPak::add_file(const std::string& name, const std::vector<uint8_t>& data) {
    if (name.empty() || data.empty()) {
        return false;
    }

    auto entry = std::make_unique<RBKEntry>();
    entry->original_name = name;
    entry->name = obfuscate_filename(name);
    entry->name_hash = hash_filename(name);
    entry->data = data;
    entry->uncompressed_size = static_cast<uint32_t>(data.size());
    entry->crc32 = calculate_crc32(data.data(), data.size());
    entry->is_loaded = true;
    entry->is_encrypted = false;

    entries_[name] = std::move(entry);
    return true;
}

bool RBPak::add_encrypted_file(const std::string& name, const std::vector<uint8_t>& data) {
    if (!add_file(name, data)) {
        return false;
    }

    entries_[name]->is_encrypted = RBKConfig::ENCRYPTION_ENABLED;
    return true;
}

bool RBPak::add_file_from_disk(const std::string& name, const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
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

bool RBPak::save_to_file(const std::string& file_path) {
    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    uint32_t signature = RBK_SIGNATURE;
    uint32_t entry_count = static_cast<uint32_t>(entries_.size());
    uint32_t directory_offset = 0;
    uint32_t config_flags = 0;

    if (RBKConfig::ENCRYPTION_ENABLED) config_flags |= 0x01;
    if (RBKConfig::OBFUSCATE_FILENAMES) config_flags |= 0x02;
    if (RBKConfig::VERIFY_CHECKSUMS) config_flags |= 0x04;

    file.write(reinterpret_cast<const char*>(&signature), 4);
    file.write(reinterpret_cast<const char*>(&entry_count), 4);
    file.write(reinterpret_cast<const char*>(&directory_offset), 4);
    file.write(reinterpret_cast<const char*>(&config_flags), 4);

    std::vector<std::pair<std::string, RBKEntry*>> sorted_entries;
    for (auto& pair : entries_) {
        sorted_entries.push_back({ pair.first, pair.second.get() });
    }

    uint32_t current_offset = 16;
    for (auto& pair : sorted_entries) {
        RBKEntry* entry = pair.second;

        std::vector<uint8_t> processed_data = entry->data;

        if (entry->is_encrypted) {
            std::vector<uint8_t> encrypted_data;
            if (!encrypt_data(processed_data.data(), processed_data.size(), encrypted_data)) {
                return false;
            }
            processed_data = encrypted_data;
        }

        std::vector<uint8_t> compressed_data;
        if (!compress_data(processed_data.data(), processed_data.size(), compressed_data)) {
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

        std::string stored_name = RBKConfig::OBFUSCATE_FILENAMES ? entry->name : entry->original_name;
        uint16_t name_length = static_cast<uint16_t>(stored_name.length());

        file.write(reinterpret_cast<const char*>(&name_length), 2);
        file.write(stored_name.c_str(), name_length);
        file.write(reinterpret_cast<const char*>(&entry->name_hash), 4);
        file.write(reinterpret_cast<const char*>(&entry->offset), 4);
        file.write(reinterpret_cast<const char*>(&entry->compressed_size), 4);
        file.write(reinterpret_cast<const char*>(&entry->uncompressed_size), 4);
        file.write(reinterpret_cast<const char*>(&entry->crc32), 4);

        uint8_t flags = entry->is_encrypted ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&flags), 1);
    }

    file.seekp(8);
    file.write(reinterpret_cast<const char*>(&directory_offset), 4);

    file.close();
    return true;
}

uint32_t murmur_hash3(const void* key, size_t len, uint32_t seed = 0x9747b28c) {
    const uint8_t* data = static_cast<const uint8_t*>(key);
    const int nblocks = static_cast<int>(len / 4);

    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data + nblocks * 4);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> (32 - 13));
        h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t* tail = data + nblocks * 4;
    uint32_t k1 = 0;

    switch (len & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2;
        h1 ^= k1;
    }

    h1 ^= static_cast<uint32_t>(len);
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

uint32_t RBPak::hash_murmur_filename(const std::string& filename) {
    return murmur_hash3(filename.data(), filename.size());
}

bool RBPak::load_from_file(const std::string& file_path) {
    clear();

    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    file_stream_.open(file_path, std::ios::binary);
    if (!file_stream_.is_open()) {
        return false;
    }

    file_path_ = file_path;

    uint32_t signature, entry_count, directory_offset, config_flags;
    file_stream_.read(reinterpret_cast<char*>(&signature), 4);
    file_stream_.read(reinterpret_cast<char*>(&entry_count), 4);
    file_stream_.read(reinterpret_cast<char*>(&directory_offset), 4);
    file_stream_.read(reinterpret_cast<char*>(&config_flags), 4);

    if (signature != RBK_SIGNATURE) {
        return false;
    }

    file_stream_.seekg(directory_offset);
    for (uint32_t i = 0; i < entry_count; i++) {
        auto entry = std::make_unique<RBKEntry>();

        uint16_t name_length;
        file_stream_.read(reinterpret_cast<char*>(&name_length), 2);

        std::string stored_name(name_length, '\0');
        file_stream_.read(&stored_name[0], name_length);

        file_stream_.read(reinterpret_cast<char*>(&entry->name_hash), 4);
        file_stream_.read(reinterpret_cast<char*>(&entry->offset), 4);
        file_stream_.read(reinterpret_cast<char*>(&entry->compressed_size), 4);
        file_stream_.read(reinterpret_cast<char*>(&entry->uncompressed_size), 4);
        file_stream_.read(reinterpret_cast<char*>(&entry->crc32), 4);

        uint8_t flags;
        file_stream_.read(reinterpret_cast<char*>(&flags), 1);
        entry->is_encrypted = (flags & 1) != 0;

        entry->name = stored_name;
        entry->is_loaded = false;

        std::string key = stored_name; 
        entries_[key] = std::move(entry);
    }

    return true;
}

bool RBPak::has_file(const std::string& name) const {
    return entries_.find(name) != entries_.end();
}

std::vector<uint8_t> RBPak::get_file(const std::string& name) {
    auto it = entries_.find(name);
    if (it == entries_.end()) {
        return std::vector<uint8_t>();
    }

    RBKEntry* entry = it->second.get();

    if (!entry->is_loaded && file_stream_.is_open()) {
        std::vector<uint8_t> compressed_data(entry->compressed_size);
        file_stream_.seekg(entry->offset);
        file_stream_.read(reinterpret_cast<char*>(compressed_data.data()), entry->compressed_size);

        std::vector<uint8_t> decompressed_data;
        if (!decompress_data(compressed_data.data(), compressed_data.size(),
            decompressed_data, entry->uncompressed_size)) {
            return std::vector<uint8_t>();
        }

        if (entry->is_encrypted) {
            std::vector<uint8_t> decrypted_data;
            if (!decrypt_data(decompressed_data.data(), decompressed_data.size(), decrypted_data)) {
                return std::vector<uint8_t>();
            }
            entry->data = decrypted_data;
        }
        else {
            entry->data = decompressed_data;
        }

        if (RBKConfig::VERIFY_CHECKSUMS) {
            uint32_t calculated_crc = calculate_crc32(entry->data.data(), entry->data.size());
            if (calculated_crc != entry->crc32) {
                return std::vector<uint8_t>();
            }
        }

        entry->is_loaded = true;
    }

    return entry->data;
}

std::vector<std::string> RBPak::get_file_list() const {
    std::vector<std::string> file_list;
    file_list.reserve(entries_.size());

    for (const auto& pair : entries_) {
        file_list.push_back(pair.first);
    }

    return file_list;
}

size_t RBPak::get_file_count() const {
    return entries_.size();
}

void RBPak::clear() {
    entries_.clear();
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    file_path_.clear();
}

std::vector<uint8_t> RBPak::operator[](const std::string& name) {
    return get_file(name);
}

void RBPak::print_config_info() {
    std::cout << "RBPak Configuration:\n";
    std::cout << "  Encryption: " << (RBKConfig::ENCRYPTION_ENABLED ? "ON" : "OFF") << "\n";
    std::cout << "  Method: " << RBKConfig::ENCRYPTION_METHOD << "\n";
    std::cout << "  Compression Level: " << RBKConfig::COMPRESSION_LEVEL << "\n";
    std::cout << "  Obfuscate Names: " << (RBKConfig::OBFUSCATE_FILENAMES ? "ON" : "OFF") << "\n";
    std::cout << "  Verify Checksums: " << (RBKConfig::VERIFY_CHECKSUMS ? "ON" : "OFF") << "\n";
}