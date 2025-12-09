#ifndef PROVER_CACHE_H
#define PROVER_CACHE_H

#include <cstdint>
#include <string>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
#include "binfile_utils.hpp"
#include <alt_bn128.hpp>

class ProverCacheWriter {
    const char*    signature = "cach";
    const uint32_t version = 1;

public:
    ProverCacheWriter(const std::string& fileName, uint32_t nSections)
        : file(fileName, std::ios::binary)
    {
        if (!file) {
            throw std::runtime_error("Failed to open file " + fileName);
        }

        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        write(signature);
        write(version);
        write(nSections);
    }

    template <typename T>
    void writeSection(uint32_t sectionId, const T *values, uint64_t count)
    {
        writeSection(sectionId, count * sizeof(T), values);
    }

    template <typename T>
    void writeSection(uint32_t sectionId, const T *value)
    {
        writeSection(sectionId, sizeof(T), value);
    }

private:
    void write(const char* value)
    {
        file << value;
    }

    template <typename T>
    void write(T value)
    {
        file.write(reinterpret_cast<char*>(&value), sizeof(T));
    }

    void write(const void *data, size_t size)
    {
        file.write(reinterpret_cast<const char*>(data), size);
    }

    void writeSection(uint32_t sectionId, uint64_t sectionLength, const void *data)
    {
        write(sectionId);
        write(sectionLength);
        write(data, sectionLength);
    }

private:
    std::ofstream file;
};

class ProverCache
{
public:
    explicit ProverCache(const std::string& cacheFilename)
        : filename(cacheFilename)
    {}

    bool isLoaded() const { return file != nullptr; }

    bool isValid(uint64_t witnessSizeExpected)
    {
        return isLoaded() && witnessSizeExpected == getWitnessSize();
    }

    uint64_t getWitnessSize() { return getSectionSize(0) / sizeof(AltBn128::FrElement); }

    auto getWitness() { return (AltBn128::FrElement*)getSectionData(0); }
    auto getPointA( ) { return (AltBn128::G1Point*)getSectionData(1); }
    auto getPointB1() { return (AltBn128::G1Point*)getSectionData(2); }
    auto getPointB2() { return (AltBn128::G2Point*)getSectionData(3); }
    auto getPointC()  { return (AltBn128::G1Point*)getSectionData(4); }

    void load()
    {
        if (exists()) {
            file = nullptr;
            file = BinFileUtils::openExisting(filename, "cach", 1);
        }
    }

    void store(AltBn128::FrElement *witness,
               size_t               witnessSize,
               AltBn128::G1Point   *pointA,
               AltBn128::G1Point   *pointB1,
               AltBn128::G2Point   *pointB2,
               AltBn128::G1Point   *pointC)
    {
        if (filename.empty()) {
            return;
        }

        file = nullptr;

        ProverCacheWriter w(filename, 5);

        w.writeSection(0, witness, witnessSize);
        w.writeSection(1, pointA);
        w.writeSection(2, pointB1);
        w.writeSection(3, pointB2);
        w.writeSection(4, pointC);
    }

private:
    bool exists() const
    {
        if (filename.empty()) {
            return false;
        }

        struct stat sb;

        if (stat(filename.c_str(), &sb) == 0) {
            return true;
        }

        if (errno == ENOENT) {
            return false;
        }

        throw std::system_error(errno, std::generic_category(), "Cannot stat prover cache");
    }

    uint64_t getSectionSize(uint32_t sectionId) {
        assert(isLoaded() && "Cannot access unloaded prover cache");

        return file->getSectionSize(sectionId);
    }

    void* getSectionData(uint32_t sectionId) {
        assert(isLoaded() && "Cannot access unloaded prover cache");

        return file->getSectionData(sectionId);
    }

private:
    std::string filename;
    std::unique_ptr<BinFileUtils::BinFile> file;
};

#endif // PROVER_CACHE_H
