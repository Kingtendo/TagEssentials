#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <compressapi.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#pragma comment(lib, "cabinet.lib")

namespace fs = std::filesystem;

constexpr char kBundleMagic[] = "TEBNDL01";
constexpr std::uint32_t kBundleFormatVersion = 1;
constexpr std::uint32_t kCompressionRaw = 0;
constexpr std::uint32_t kCompressionXpressHuff = 1;

void WriteU32(std::ofstream& output, std::uint32_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void WriteU64(std::ofstream& output, std::uint64_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool ReadFile(const fs::path& path, std::vector<std::uint8_t>& bytes) {
    std::error_code error;
    std::uintmax_t fileSize = fs::file_size(path, error);
    if (error || fileSize > static_cast<std::uintmax_t>(std::numeric_limits<size_t>::max())) {
        std::cerr << "Could not read file size: " << path.string() << "\n";
        return false;
    }

    bytes.resize(static_cast<size_t>(fileSize));
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        std::cerr << "Could not open: " << path.string() << "\n";
        return false;
    }

    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            std::cerr << "Could not read: " << path.string() << "\n";
            return false;
        }
    }
    return true;
}

bool CompressBytes(
    COMPRESSOR_HANDLE compressor,
    const std::vector<std::uint8_t>& input,
    std::vector<std::uint8_t>& output,
    std::uint32_t& compressionType) {
    if (input.empty()) {
        output.clear();
        compressionType = kCompressionRaw;
        return true;
    }

    size_t capacity = std::max<size_t>(input.size() + 65536, 4096);
    for (int attempt = 0; attempt < 8; ++attempt) {
        output.resize(capacity);
        SIZE_T compressedSize = 0;
        if (Compress(
            compressor,
            input.data(),
            input.size(),
            output.data(),
            output.size(),
            &compressedSize)) {
            output.resize(static_cast<size_t>(compressedSize));
            if (output.size() < input.size()) {
                compressionType = kCompressionXpressHuff;
            } else {
                output = input;
                compressionType = kCompressionRaw;
            }
            return true;
        }

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || capacity > (std::numeric_limits<size_t>::max() / 2)) {
            std::cerr << "Windows compression failed with error " << GetLastError() << "\n";
            return false;
        }
        capacity *= 2;
    }

    std::cerr << "Windows compression buffer could not be sized.\n";
    return false;
}

struct BundleFile {
    fs::path absolutePath;
    std::string relativePath;
};

bool EnumerateFiles(const fs::path& inputDirectory, std::vector<BundleFile>& files) {
    std::error_code error;
    fs::recursive_directory_iterator iterator(
        inputDirectory,
        fs::directory_options::skip_permission_denied,
        error);
    if (error) {
        std::cerr << "Could not enumerate input directory: " << error.message() << "\n";
        return false;
    }

    const fs::recursive_directory_iterator end;
    for (; iterator != end; iterator.increment(error)) {
        if (error) {
            std::cerr << "Could not enumerate input directory: " << error.message() << "\n";
            return false;
        }

        std::error_code statusError;
        if (!iterator->is_regular_file(statusError) || statusError) continue;

        fs::path relative = fs::relative(iterator->path(), inputDirectory, error);
        if (error) {
            std::cerr << "Could not resolve relative path: " << error.message() << "\n";
            return false;
        }

        std::string relativePath = relative.generic_u8string();
        if (relativePath.empty() || relativePath.size() > std::numeric_limits<std::uint32_t>::max()) {
            std::cerr << "Invalid runtime file path.\n";
            return false;
        }

        files.push_back({ iterator->path(), std::move(relativePath) });
    }

    std::sort(files.begin(), files.end(), [](const BundleFile& left, const BundleFile& right) {
        return left.relativePath < right.relativePath;
    });
    return true;
}

bool WriteBundle(const fs::path& inputDirectory, const fs::path& outputPath) {
    std::vector<BundleFile> files;
    if (!EnumerateFiles(inputDirectory, files)) return false;
    if (files.size() > std::numeric_limits<std::uint32_t>::max()) {
        std::cerr << "Runtime bundle contains too many files.\n";
        return false;
    }

    COMPRESSOR_HANDLE compressor = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor)) {
        std::cerr << "Could not create Windows compressor. Error " << GetLastError() << "\n";
        return false;
    }

    fs::path temporaryPath = outputPath;
    temporaryPath += L".tmp";
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        CloseCompressor(compressor);
        std::cerr << "Could not create bundle output: " << outputPath.string() << "\n";
        return false;
    }

    output.write(kBundleMagic, sizeof(kBundleMagic) - 1);
    WriteU32(output, kBundleFormatVersion);
    WriteU32(output, kCompressionXpressHuff);
    WriteU32(output, static_cast<std::uint32_t>(files.size()));
    WriteU32(output, 0);

    std::vector<std::uint8_t> inputBytes;
    std::vector<std::uint8_t> compressedBytes;
    for (const BundleFile& file : files) {
        if (!ReadFile(file.absolutePath, inputBytes)) {
            output.close();
            CloseCompressor(compressor);
            DeleteFileW(temporaryPath.c_str());
            return false;
        }

        std::uint32_t compressionType = kCompressionRaw;
        if (!CompressBytes(compressor, inputBytes, compressedBytes, compressionType)) {
            output.close();
            CloseCompressor(compressor);
            DeleteFileW(temporaryPath.c_str());
            return false;
        }

        WriteU32(output, static_cast<std::uint32_t>(file.relativePath.size()));
        WriteU32(output, compressionType);
        WriteU64(output, static_cast<std::uint64_t>(inputBytes.size()));
        WriteU64(output, static_cast<std::uint64_t>(compressedBytes.size()));
        output.write(file.relativePath.data(), static_cast<std::streamsize>(file.relativePath.size()));
        if (!compressedBytes.empty()) {
            output.write(
                reinterpret_cast<const char*>(compressedBytes.data()),
                static_cast<std::streamsize>(compressedBytes.size()));
        }

        if (!output) {
            output.close();
            CloseCompressor(compressor);
            DeleteFileW(temporaryPath.c_str());
            std::cerr << "Could not write runtime bundle.\n";
            return false;
        }
    }

    output.flush();
    bool writeSucceeded = static_cast<bool>(output);
    output.close();
    CloseCompressor(compressor);
    if (!writeSucceeded) {
        DeleteFileW(temporaryPath.c_str());
        std::cerr << "Could not finish runtime bundle.\n";
        return false;
    }

    if (!MoveFileExW(temporaryPath.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(temporaryPath.c_str());
        std::cerr << "Could not move runtime bundle into place. Error " << GetLastError() << "\n";
        return false;
    }
    return true;
}

bool VerifyBundle(const fs::path& bundlePath) {
    std::vector<std::uint8_t> bundle;
    if (!ReadFile(bundlePath, bundle)) return false;
    const std::uint8_t* cursor = bundle.data();
    const std::uint8_t* end = cursor + bundle.size();
    auto readU32 = [&cursor, end](std::uint32_t& value) {
        if (static_cast<size_t>(end - cursor) < sizeof(value)) return false;
        std::memcpy(&value, cursor, sizeof(value));
        cursor += sizeof(value);
        return true;
    };
    auto readU64 = [&cursor, end](std::uint64_t& value) {
        if (static_cast<size_t>(end - cursor) < sizeof(value)) return false;
        std::memcpy(&value, cursor, sizeof(value));
        cursor += sizeof(value);
        return true;
    };

    if (bundle.size() < 24 || std::memcmp(cursor, kBundleMagic, sizeof(kBundleMagic) - 1) != 0) {
        std::cerr << "Bundle header is invalid.\n";
        return false;
    }
    cursor += sizeof(kBundleMagic) - 1;
    std::uint32_t formatVersion = 0;
    std::uint32_t algorithm = 0;
    std::uint32_t fileCount = 0;
    std::uint32_t reserved = 0;
    if (!readU32(formatVersion) || !readU32(algorithm) || !readU32(fileCount) || !readU32(reserved) ||
        formatVersion != kBundleFormatVersion || algorithm != kCompressionXpressHuff) {
        std::cerr << "Bundle header values are invalid.\n";
        return false;
    }

    DECOMPRESSOR_HANDLE decompressor = nullptr;
    for (std::uint32_t index = 0; index < fileCount; ++index) {
        std::uint32_t pathLength = 0;
        std::uint32_t compressionType = 0;
        std::uint64_t uncompressedSize = 0;
        std::uint64_t payloadSize = 0;
        if (!readU32(pathLength) || !readU32(compressionType) ||
            !readU64(uncompressedSize) || !readU64(payloadSize) ||
            pathLength > 32768 || payloadSize > static_cast<std::uint64_t>(end - cursor)) {
            if (decompressor) CloseDecompressor(decompressor);
            std::cerr << "Bundle entry header is invalid at index " << index << ".\n";
            return false;
        }

        std::string path(reinterpret_cast<const char*>(cursor), pathLength);
        cursor += pathLength;
        if (payloadSize > static_cast<std::uint64_t>(end - cursor)) {
            if (decompressor) CloseDecompressor(decompressor);
            std::cerr << "Bundle entry is truncated: " << path << "\n";
            return false;
        }
        const std::uint8_t* payload = cursor;
        cursor += static_cast<size_t>(payloadSize);

        if (compressionType == kCompressionXpressHuff) {
            if (!decompressor && !CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor)) {
                std::cerr << "Could not create verifier decompressor. Error " << GetLastError() << "\n";
                return false;
            }
            if (uncompressedSize > std::numeric_limits<size_t>::max()) {
                CloseDecompressor(decompressor);
                std::cerr << "Bundle entry is too large: " << path << "\n";
                return false;
            }
            std::vector<std::uint8_t> output(static_cast<size_t>(uncompressedSize));
            SIZE_T actualSize = 0;
            if (!Decompress(
                decompressor,
                payload,
                static_cast<SIZE_T>(payloadSize),
                output.empty() ? nullptr : output.data(),
                output.size(),
                &actualSize) || actualSize != output.size()) {
                std::cerr << "Bundle entry failed to decompress: " << path <<
                    " error " << GetLastError() << " expected " << uncompressedSize <<
                    " actual " << actualSize << "\n";
                CloseDecompressor(decompressor);
                return false;
            }
        } else if (compressionType != kCompressionRaw || payloadSize != uncompressedSize) {
            if (decompressor) CloseDecompressor(decompressor);
            std::cerr << "Bundle entry compression is invalid: " << path << "\n";
            return false;
        }
    }

    if (decompressor) CloseDecompressor(decompressor);
    if (cursor != end) {
        std::cerr << "Bundle contains trailing data.\n";
        return false;
    }
    std::cout << "Verified " << fileCount << " runtime files.\n";
    return true;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc == 3 && _wcsicmp(argv[1], L"--verify") == 0) {
        return VerifyBundle(fs::path(argv[2])) ? 0 : 1;
    }
    if (argc != 5 || _wcsicmp(argv[1], L"--input") != 0 || _wcsicmp(argv[3], L"--output") != 0) {
        std::wcerr << L"Usage: RuntimeBundlePacker.exe --input <directory> --output <file>\n";
        return 2;
    }

    fs::path inputDirectory(argv[2]);
    fs::path outputPath(argv[4]);
    std::error_code error;
    if (!fs::is_directory(inputDirectory, error) || error) {
        std::wcerr << L"Input directory does not exist: " << inputDirectory.c_str() << L"\n";
        return 2;
    }
    if (outputPath.has_parent_path()) fs::create_directories(outputPath.parent_path(), error);
    if (error) {
        std::wcerr << L"Could not create output directory: " << error.message().c_str() << L"\n";
        return 2;
    }

    return WriteBundle(inputDirectory, outputPath) ? 0 : 1;
}
