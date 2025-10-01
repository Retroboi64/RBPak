/*
 * RBPak - Retroboi64's Package System
 * A compact yet powerful file packaging system for game engines
 * Copyright (c) 2025 Patrick Reese (Retroboi64)
 *
 * Licensed under MIT
 * See LICENSE file for full terms
 * GitHub: https://github.com/Retroboi64/RBPak
 */
#include "pak.h"
#include <zlib.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <cstring>
#include <list>
#include <mutex>
#include <atomic>

namespace fs = std::filesystem;

namespace rbpak {
    template<typename Key, typename Value>
    class LRUCache {
    private:
        size_t m_capacity;
        size_t m_current_size{ 0 };
        std::list<std::pair<Key, Value>> m_items;
        std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> m_map;
        mutable std::mutex m_mutex;

    public:
        explicit LRUCache(size_t capacity) : m_capacity(capacity) {}

        std::optional<Value> Get(const Key& key) {
            std::lock_guard lock(m_mutex);
            auto it = m_map.find(key);
            if (it == m_map.end()) return std::nullopt;
            m_items.splice(m_items.begin(), m_items, it->second);
            return it->second->second;
        }

        void Put(const Key& key, Value value, size_t size) {
            std::lock_guard lock(m_mutex);
            auto it = m_map.find(key);
            if (it != m_map.end()) {
                m_items.erase(it->second);
                m_map.erase(it);
            }
            while (m_current_size + size > m_capacity && !m_items.empty()) {
                m_map.erase(m_items.back().first);
                m_items.pop_back();
            }
            if (size <= m_capacity) {
                m_items.emplace_front(key, std::move(value));
                m_map[key] = m_items.begin();
                m_current_size += size;
            }
        }

        void Clear() {
            std::lock_guard lock(m_mutex);
            m_items.clear();
            m_map.clear();
            m_current_size = 0;
        }

        size_t Size() const {
            std::lock_guard lock(m_mutex);
            return m_current_size;
        }
    };

    struct Entry {
        std::string name;
        std::string stored_name;
        uint32_t offset{ 0 };
        uint32_t compressed_size{ 0 };
        uint32_t uncompressed_size{ 0 };
        uint32_t crc32{ 0 };
        bool is_encrypted{ false };
        bool is_loaded{ false };
        ByteArray data;
    };

    class Cipher {
    public:
        explicit Cipher(std::string_view key) {
            if (key.empty()) {
                m_key = { 0x52, 0x42, 0x50, 0x6B };
            }
            else {
                DeriveKey(key);
            }
        }

        void Encrypt(uint8_t* data, size_t size) const {
            if (m_key.empty() || !data) return;
            for (size_t i = 0; i < size; ++i) {
                data[i] ^= m_key[i % m_key.size()];
            }
        }

        void Decrypt(uint8_t* data, size_t size) const {
            Encrypt(data, size);
        }

    private:
        void DeriveKey(std::string_view input) {
            m_key.clear();
            m_key.reserve(32);
            std::string seed = std::string(input) + "RBPak_Salt_2025";
            for (size_t i = 0; i < 32; ++i) {
                uint32_t hash = 2166136261u;
                for (char c : seed) {
                    hash ^= static_cast<uint32_t>(static_cast<uint8_t>(c));
                    hash *= 16777619u;
                }
                m_key.push_back(static_cast<uint8_t>(hash & 0xFF));
                seed += std::to_string(hash);
            }
        }
        ByteArray m_key;
    };

    namespace compression {
        PackageResult Compress(const uint8_t* input, size_t input_size, ByteArray& output, CompressionLevel level) {
            if (!input || input_size == 0) {
                return PackageResult::Failure(PackageError::InvalidParameter, "Empty input");
            }
            if (level == CompressionLevel::None) {
                output.assign(input, input + input_size);
                return PackageResult::Success();
            }
            uLongf bound = compressBound(static_cast<uLong>(input_size));
            output.resize(bound);
            int result = compress2(output.data(), &bound, input, static_cast<uLong>(input_size), static_cast<int>(level));
            if (result != Z_OK) {
                return PackageResult::Failure(PackageError::CompressionFailed, "zlib error: " + std::to_string(result));
            }
            output.resize(bound);
            return PackageResult::Success();
        }

        PackageResult Decompress(const uint8_t* input, size_t input_size, ByteArray& output, size_t expected) {
            if (!input || input_size == 0) {
                return PackageResult::Failure(PackageError::InvalidParameter, "Empty compressed data");
            }
            if (expected == 0 || expected > 1024ULL * 1024 * 1024) {
                return PackageResult::Failure(PackageError::InvalidParameter, "Invalid size");
            }
            output.resize(expected);
            uLongf size = static_cast<uLongf>(expected);
            int result = uncompress(output.data(), &size, input, static_cast<uLong>(input_size));
            if (result != Z_OK) {
                return PackageResult::Failure(PackageError::DecompressionFailed, "zlib error: " + std::to_string(result));
            }
            output.resize(size);
            return PackageResult::Success();
        }
    }

    namespace hash {
        uint32_t MurmurHash3(const void* key, size_t len, uint32_t seed = 0x52425061) {
            if (!key || len == 0) return seed;
            const uint8_t* data = static_cast<const uint8_t*>(key);
            const int nblocks = static_cast<int>(len / 4);
            uint32_t h1 = seed;
            const uint32_t c1 = 0xcc9e2d51;
            const uint32_t c2 = 0x1b873593;
            const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data + nblocks * 4);
            for (int i = -nblocks; i; i++) {
                uint32_t k1 = blocks[i];
                k1 *= c1;
                k1 = (k1 << 15) | (k1 >> 17);
                k1 *= c2;
                h1 ^= k1;
                h1 = (h1 << 13) | (h1 >> 19);
                h1 = h1 * 5 + 0xe6546b64;
            }
            const uint8_t* tail = data + nblocks * 4;
            uint32_t k1 = 0;
            switch (len & 3) {
            case 3: k1 ^= tail[2] << 16; [[fallthrough]];
            case 2: k1 ^= tail[1] << 8; [[fallthrough]];
            case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << 15) | (k1 >> 17);
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

        std::string Obfuscate(std::string_view name) {
            uint32_t hash = MurmurHash3(name.data(), name.size());
            return "rbp_" + std::to_string(hash) + ".dat";
        }
    }

    class IOHelper {
    public:
        template<typename T>
        static bool Write(std::ostream& stream, const T& value) {
            return stream.write(reinterpret_cast<const char*>(&value), sizeof(T)).good();
        }

        template<typename T>
        static bool Read(std::istream& stream, T& value) {
            return stream.read(reinterpret_cast<char*>(&value), sizeof(T)).good();
        }

        static bool WriteString(std::ostream& stream, std::string_view str) {
            if (str.size() > UINT16_MAX) return false;
            uint16_t length = static_cast<uint16_t>(str.length());
            if (!Write(stream, length)) return false;
            return stream.write(str.data(), length).good();
        }

        static bool ReadString(std::istream& stream, std::string& str) {
            uint16_t length;
            if (!Read(stream, length)) return false;
            if (length > 8192) return false;
            str.resize(length);
            return stream.read(str.data(), length).good();
        }
    };

    class Package::Impl {
    private:
        static constexpr uint32_t SIGNATURE = 0x6B506252;
        static constexpr uint32_t VERSION = 0x00020000;

        PackageConfig m_config;
        std::unordered_map<std::string, std::unique_ptr<Entry>> m_entries;
        std::string m_filepath;
        mutable std::ifstream m_reader;
        std::unique_ptr<Cipher> m_cipher;
        LRUCache<std::string, ByteArray> m_cache;
        mutable std::atomic<PackageError> m_last_error{ PackageError::None };

    public:
        explicit Impl(const PackageConfig& config) : m_config(config), m_cache(config.max_cache_size) {
            if (m_config.encryption != EncryptionMethod::None && !m_config.encryption_key.empty()) {
                m_cipher = std::make_unique<Cipher>(m_config.encryption_key);
            }
        }

        ~Impl() {
            if (m_reader.is_open()) m_reader.close();
        }

        PackageResult Add(std::string_view name, const uint8_t* data, size_t size) {
            if (name.empty() || !data || size == 0) {
                return PackageResult::Failure(PackageError::InvalidParameter, "Invalid parameters");
            }
            auto entry = std::make_unique<Entry>();
            entry->name = name;
            entry->stored_name = m_config.obfuscate_filenames ? hash::Obfuscate(name) : std::string(name);
            entry->data.assign(data, data + size);
            entry->uncompressed_size = static_cast<uint32_t>(size);
            entry->crc32 = pak_utils::CalculateCRC32(data, size);
            entry->is_encrypted = (m_config.encryption != EncryptionMethod::None);
            entry->is_loaded = true;
            m_entries[std::string(name)] = std::move(entry);
            return PackageResult::Success();
        }

        PackageResult AddFromFile(std::string_view name, std::string_view filepath) {
            std::ifstream file(std::string(filepath), std::ios::binary);
            if (!file.is_open()) {
                return PackageResult::Failure(PackageError::FileNotFound, "Cannot open file");
            }
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            ByteArray data(size);
            if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
                return PackageResult::Failure(PackageError::IOError, "Cannot read file");
            }
            return Add(name, data.data(), data.size());
        }

        PackageResult AddDirectory(std::string_view directory, bool recursive, ProgressCallback callback) {
            std::string dir_str(directory);
            if (!fs::exists(dir_str) || !fs::is_directory(dir_str)) {
                return PackageResult::Failure(PackageError::FileNotFound, "Directory not found");
            }
            try {
                std::vector<fs::path> files;
                if (recursive) {
                    for (const auto& entry : fs::recursive_directory_iterator(dir_str)) {
                        if (entry.is_regular_file()) files.push_back(entry.path());
                    }
                }
                else {
                    for (const auto& entry : fs::directory_iterator(dir_str)) {
                        if (entry.is_regular_file()) files.push_back(entry.path());
                    }
                }
                size_t current = 0;
                for (const auto& file : files) {
                    std::string relative = fs::relative(file, dir_str).string();
                    if (callback) callback(current++, files.size(), relative);
                    if (auto result = AddFromFile(relative, file.string()); !result) {
                        std::cerr << "Failed to add: " << relative << std::endl;
                    }
                }
                return PackageResult::Success();
            }
            catch (const std::exception& e) {
                return PackageResult::Failure(PackageError::IOError, e.what());
            }
        }

        PackageResult AddMultiple(const std::vector<std::pair<std::string, ByteArray>>& files, ProgressCallback callback) {
            size_t current = 0;
            for (const auto& [name, data] : files) {
                if (callback) callback(current++, files.size(), name);
                if (auto result = Add(name, data.data(), data.size()); !result) {
                    return result;
                }
            }
            return PackageResult::Success();
        }

        std::optional<ByteArray> Get(std::string_view name) {
            std::string key(name);
            if (auto cached = m_cache.Get(key)) return cached;
            auto it = m_entries.find(key);
            if (it == m_entries.end()) return std::nullopt;
            Entry* entry = it->second.get();
            if (!entry->is_loaded) {
                if (auto result = LoadEntry(entry); !result) return std::nullopt;
            }
            if (m_config.lazy_load) {
                m_cache.Put(key, entry->data, entry->data.size());
            }
            return entry->data;
        }

        PackageResult Extract(std::string_view name, std::string_view output_path) {
            auto data = Get(name);
            if (!data) return PackageResult::Failure(PackageError::FileNotFound, "File not found");
            std::ofstream file(std::string(output_path), std::ios::binary);
            if (!file.is_open()) return PackageResult::Failure(PackageError::IOError, "Cannot create file");
            if (!file.write(reinterpret_cast<const char*>(data->data()), data->size())) {
                return PackageResult::Failure(PackageError::IOError, "Write failed");
            }
            return PackageResult::Success();
        }

        PackageResult ExtractAll(std::string_view output_dir, ProgressCallback callback) {
            std::string dir(output_dir);
            fs::create_directories(dir);
            size_t current = 0;
            size_t total = m_entries.size();
            for (const auto& [name, entry] : m_entries) {
                if (callback) callback(current++, total, name);
                fs::path output_path = fs::path(dir) / name;
                fs::create_directories(output_path.parent_path());
                if (auto result = Extract(name, output_path.string()); !result) {
                    return result;
                }
            }
            return PackageResult::Success();
        }

        bool Remove(std::string_view name) {
            return m_entries.erase(std::string(name)) > 0;
        }

        bool Has(std::string_view name) const {
            return m_entries.find(std::string(name)) != m_entries.end();
        }

        std::optional<FileInfo> GetFileInfo(std::string_view name) const {
            auto it = m_entries.find(std::string(name));
            if (it == m_entries.end()) return std::nullopt;
            const Entry* entry = it->second.get();
            return FileInfo{ entry->name, entry->stored_name, entry->uncompressed_size,
                          entry->compressed_size, entry->crc32, entry->is_encrypted, entry->is_loaded };
        }

        PackageResult Save(std::string_view filepath, ProgressCallback callback) {
            std::ofstream file(std::string(filepath), std::ios::binary);
            if (!file.is_open()) return PackageResult::Failure(PackageError::IOError, "Cannot create package");

            IOHelper::Write(file, SIGNATURE);
            IOHelper::Write(file, VERSION);
            IOHelper::Write(file, static_cast<uint32_t>(m_entries.size()));

            uint32_t flags = 0;
            if (m_config.compression != CompressionLevel::None) flags |= static_cast<uint32_t>(PackageFlags::Compressed);
            if (m_config.encryption != EncryptionMethod::None) flags |= static_cast<uint32_t>(PackageFlags::Encrypted);
            if (m_config.obfuscate_filenames) flags |= static_cast<uint32_t>(PackageFlags::ObfuscatedNames);
            if (m_config.verify_checksums) flags |= static_cast<uint32_t>(PackageFlags::ChecksumVerified);
            IOHelper::Write(file, flags);

            size_t dir_offset_pos = file.tellp();
            IOHelper::Write(file, uint32_t(0));

            std::vector<Entry*> sorted;
            for (auto& [_, entry] : m_entries) sorted.push_back(entry.get());

            size_t current = 0;
            for (auto* entry : sorted) {
                if (callback) callback(current++, sorted.size(), entry->name);
                ByteArray processed = entry->data;
                if (entry->is_encrypted && m_cipher) {
                    m_cipher->Encrypt(processed.data(), processed.size());
                }
                ByteArray compressed;
                if (auto result = compression::Compress(processed.data(), processed.size(), compressed, m_config.compression); !result) {
                    return result;
                }
                entry->offset = static_cast<uint32_t>(file.tellp());
                entry->compressed_size = static_cast<uint32_t>(compressed.size());
                file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
            }

            uint32_t dir_offset = static_cast<uint32_t>(file.tellp());
            for (auto* entry : sorted) {
                IOHelper::WriteString(file, entry->stored_name);
                IOHelper::Write(file, entry->offset);
                IOHelper::Write(file, entry->compressed_size);
                IOHelper::Write(file, entry->uncompressed_size);
                IOHelper::Write(file, entry->crc32);
                IOHelper::Write(file, static_cast<uint8_t>(entry->is_encrypted ? 1 : 0));
            }

            file.seekp(dir_offset_pos);
            IOHelper::Write(file, dir_offset);
            return PackageResult::Success();
        }

        PackageResult Load(std::string_view filepath) {
            Clear();
            if (m_reader.is_open()) m_reader.close();
            m_reader.open(std::string(filepath), std::ios::binary);
            if (!m_reader.is_open()) {
                return PackageResult::Failure(PackageError::FileNotFound, "Cannot open package");
            }
            m_filepath = filepath;

            uint32_t sig, ver, count, flags, dir_off;
            if (!IOHelper::Read(m_reader, sig) || sig != SIGNATURE) {
                return PackageResult::Failure(PackageError::InvalidSignature, "Invalid signature");
            }
            IOHelper::Read(m_reader, ver);
            IOHelper::Read(m_reader, count);
            IOHelper::Read(m_reader, flags);
            IOHelper::Read(m_reader, dir_off);

            m_config.encryption = (flags & static_cast<uint32_t>(PackageFlags::Encrypted)) ? EncryptionMethod::XOR : EncryptionMethod::None;
            m_config.obfuscate_filenames = (flags & static_cast<uint32_t>(PackageFlags::ObfuscatedNames)) != 0;
            m_config.verify_checksums = (flags & static_cast<uint32_t>(PackageFlags::ChecksumVerified)) != 0;

            m_reader.seekg(dir_off);
            for (uint32_t i = 0; i < count; ++i) {
                auto entry = std::make_unique<Entry>();
                IOHelper::ReadString(m_reader, entry->stored_name);
                IOHelper::Read(m_reader, entry->offset);
                IOHelper::Read(m_reader, entry->compressed_size);
                IOHelper::Read(m_reader, entry->uncompressed_size);
                IOHelper::Read(m_reader, entry->crc32);
                uint8_t enc;
                IOHelper::Read(m_reader, enc);
                entry->is_encrypted = (enc != 0);
                entry->name = entry->stored_name;
                entry->is_loaded = false;
                m_entries[entry->name] = std::move(entry);
            }
            return PackageResult::Success();
        }

        void Clear() noexcept {
            m_entries.clear();
            m_filepath.clear();
            if (m_reader.is_open()) m_reader.close();
        }

        std::vector<std::string> List() const {
            std::vector<std::string> names;
            for (const auto& [name, _] : m_entries) names.push_back(name);
            std::sort(names.begin(), names.end());
            return names;
        }

        std::vector<FileInfo> ListDetailed() const {
            std::vector<FileInfo> infos;
            for (const auto& [_, entry] : m_entries) {
                infos.push_back({ entry->name, entry->stored_name, entry->uncompressed_size,
                               entry->compressed_size, entry->crc32, entry->is_encrypted, entry->is_loaded });
            }
            return infos;
        }

        size_t GetFileCount() const noexcept { return m_entries.size(); }

        size_t GetTotalSize() const noexcept {
            size_t total = 0;
            for (const auto& [_, entry] : m_entries) total += entry->uncompressed_size;
            return total;
        }

        size_t GetCompressedSize() const noexcept {
            size_t total = 0;
            for (const auto& [_, entry] : m_entries) total += entry->compressed_size;
            return total;
        }

        float GetCompressionRatio() const noexcept {
            size_t total = GetTotalSize();
            if (total == 0) return 0.0f;
            return 1.0f - (static_cast<float>(GetCompressedSize()) / total);
        }

        void PrintStatistics() const {
            std::cout << "\n=== RBPak Statistics ===" << std::endl;
            std::cout << "Files: " << GetFileCount() << std::endl;
            std::cout << "Total Size: " << pak_utils::FormatSize(GetTotalSize()) << std::endl;
            std::cout << "Compressed: " << pak_utils::FormatSize(GetCompressedSize()) << std::endl;
            if (GetTotalSize() > 0) {
                std::cout << "Ratio: " << std::fixed << std::setprecision(2) << (GetCompressionRatio() * 100.0f) << "%" << std::endl;
            }
            std::cout << "Encrypted: " << (m_config.encryption != EncryptionMethod::None ? "Yes" : "No") << std::endl;
            std::cout << "Obfuscated: " << (m_config.obfuscate_filenames ? "Yes" : "No") << std::endl;
        }

        const PackageConfig& GetConfig() const noexcept { return m_config; }
        PackageError GetLastError() const noexcept { return m_last_error.load(); }
        void ClearCache() noexcept { m_cache.Clear(); }
        size_t GetCacheSize() const noexcept { return m_cache.Size(); }

    private:
        PackageResult LoadEntry(Entry* entry) {
            if (!m_reader.is_open()) return PackageResult::Failure(PackageError::IOError, "Package not open");
            ByteArray compressed(entry->compressed_size);
            m_reader.seekg(entry->offset);
            if (!m_reader.read(reinterpret_cast<char*>(compressed.data()), entry->compressed_size)) {
                return PackageResult::Failure(PackageError::IOError, "Read failed");
            }
            ByteArray decompressed;
            if (auto result = compression::Decompress(compressed.data(), compressed.size(), decompressed, entry->uncompressed_size); !result) {
                return result;
            }
            if (entry->is_encrypted && m_cipher) {
                m_cipher->Decrypt(decompressed.data(), decompressed.size());
            }
            if (m_config.verify_checksums) {
                uint32_t calc = pak_utils::CalculateCRC32(decompressed.data(), decompressed.size());
                if (!pak_utils::SecureCompare(calc, entry->crc32)) {
                    return PackageResult::Failure(PackageError::ChecksumMismatch, "CRC mismatch");
                }
            }
            entry->data = std::move(decompressed);
            entry->is_loaded = true;
            return PackageResult::Success();
        }
    };

    Package::Package(const PackageConfig& config) : m_impl(std::make_unique<Impl>(config)) {}
    Package::~Package() = default;
    Package::Package(Package&&) noexcept = default;
    Package& Package::operator=(Package&&) noexcept = default;

    PackageResult Package::Add(std::string_view name, const ByteArray& data) {
        return m_impl->Add(name, data.data(), data.size());
    }

    PackageResult Package::Add(std::string_view name, std::span<const uint8_t> data) {
        return m_impl->Add(name, data.data(), data.size());
    }

    PackageResult Package::AddFromFile(std::string_view name, std::string_view filepath) {
        return m_impl->AddFromFile(name, filepath);
    }

    PackageResult Package::AddDirectory(std::string_view directory, bool recursive, ProgressCallback callback) {
        return m_impl->AddDirectory(directory, recursive, callback);
    }

    PackageResult Package::AddMultiple(const std::vector<std::pair<std::string, ByteArray>>& files, ProgressCallback callback) {
        return m_impl->AddMultiple(files, callback);
    }

    std::optional<ByteArray> Package::Get(std::string_view name) {
        return m_impl->Get(name);
    }

    PackageResult Package::Extract(std::string_view name, std::string_view output_path) {
        return m_impl->Extract(name, output_path);
    }

    PackageResult Package::ExtractAll(std::string_view output_directory, ProgressCallback callback) {
        return m_impl->ExtractAll(output_directory, callback);
    }

    bool Package::Remove(std::string_view name) {
        return m_impl->Remove(name);
    }

    bool Package::Has(std::string_view name) const {
        return m_impl->Has(name);
    }

    std::optional<FileInfo> Package::GetFileInfo(std::string_view name) const {
        return m_impl->GetFileInfo(name);
    }

    PackageResult Package::Save(std::string_view filepath, ProgressCallback callback) {
        return m_impl->Save(filepath, callback);
    }

    PackageResult Package::Load(std::string_view filepath) {
        return m_impl->Load(filepath);
    }

    void Package::Clear() noexcept {
        m_impl->Clear();
    }

    std::vector<std::string> Package::List() const {
        return m_impl->List();
    }

    std::vector<FileInfo> Package::ListDetailed() const {
        return m_impl->ListDetailed();
    }

    size_t Package::GetFileCount() const noexcept {
        return m_impl->GetFileCount();
    }

    size_t Package::GetTotalSize() const noexcept {
        return m_impl->GetTotalSize();
    }

    size_t Package::GetCompressedSize() const noexcept {
        return m_impl->GetCompressedSize();
    }

    float Package::GetCompressionRatio() const noexcept {
        return m_impl->GetCompressionRatio();
    }

    void Package::ClearCache() noexcept {
        m_impl->ClearCache();
    }

    size_t Package::GetCacheSize() const noexcept {
        return m_impl->GetCacheSize();
    }

    void Package::PrintStatistics() const {
        m_impl->PrintStatistics();
    }

    const PackageConfig& Package::GetConfig() const noexcept {
        return m_impl->GetConfig();
    }

    PackageError Package::GetLastError() const noexcept {
        return m_impl->GetLastError();
    }

    std::optional<ByteArray> Package::operator[](std::string_view name) {
        return Get(name);
    }

    namespace pak_utils {
        uint32_t CalculateCRC32(std::span<const uint8_t> data) {
            return crc32(0L, data.data(), static_cast<uInt>(data.size()));
        }

        uint32_t CalculateCRC32(const uint8_t* data, size_t size) {
            return crc32(0L, data, static_cast<uInt>(size));
        }

        std::string ObfuscateName(std::string_view name) {
            return hash::Obfuscate(name);
        }

        bool ValidatePackageFile(std::string_view filepath) {
            std::ifstream file(std::string(filepath), std::ios::binary);
            if (!file.is_open()) return false;
            constexpr uint32_t SIGNATURE = 0x6B506252;
            uint32_t sig;
            file.read(reinterpret_cast<char*>(&sig), sizeof(sig));
            return sig == SIGNATURE;
        }

        std::string FormatSize(size_t bytes) {
            constexpr const char* units[] = { "B", "KB", "MB", "GB", "TB" };
            int unit_index = 0;
            double size = static_cast<double>(bytes);
            while (size >= 1024.0 && unit_index < 4) {
                size /= 1024.0;
                ++unit_index;
            }
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
            return oss.str();
        }

        std::string GetErrorMessage(PackageError error) {
            switch (error) {
            case PackageError::None: return "No error";
            case PackageError::FileNotFound: return "File not found";
            case PackageError::InvalidSignature: return "Invalid package signature";
            case PackageError::CorruptedData: return "Data corruption detected";
            case PackageError::DecryptionFailed: return "Decryption failed";
            case PackageError::CompressionFailed: return "Compression failed";
            case PackageError::DecompressionFailed: return "Decompression failed";
            case PackageError::ChecksumMismatch: return "Checksum verification failed";
            case PackageError::IOError: return "I/O error";
            case PackageError::InvalidParameter: return "Invalid parameter";
            case PackageError::OutOfMemory: return "Out of memory";
            case PackageError::AccessDenied: return "Access denied";
            default: return "Unknown error";
            }
        }

        bool SecureCompare(uint32_t a, uint32_t b) {
            volatile uint32_t diff = a ^ b;
            return diff == 0;
        }
    }
}