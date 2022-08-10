// library headers
#include <appimage/appimage_shared.h>
#include <zshash.h>

// local headers
#include "updatableappimage.h"
#include "updateinformation/updateinformation.h"
#include "util/util.h"

namespace appimage::update {
    using namespace updateinformation;
    using namespace util;
    using namespace zsync2;

    void UpdatableAppImage::assertIfstreamGood(const std::ifstream& ifs) const {
        if (!ifs || !ifs.good()) {
            throw AppImageError("Error while opening/accessing/reading from AppImage: " + _path);
        }
    }

    std::ifstream UpdatableAppImage::_open() const  {
        // check whether file exists
        std::ifstream ifs(_path);
        assertIfstreamGood(ifs);
        return ifs;
    }

    bool UpdatableAppImage::_hasElfMagicValue(std::ifstream& ifs) const {
        static constexpr int elfMagicPos = 0;
        static const std::string elfMagicValue = "\7ELF";

        ifs.seekg(elfMagicPos);
        assertIfstreamGood(ifs);

        std::vector<char> elfMagicPosData(elfMagicValue.size() + 1, '\0');

        ifs.read(elfMagicPosData.data(), elfMagicValue.size());
        assertIfstreamGood(ifs);

        return elfMagicPosData.data() == elfMagicValue;
    }

    bool UpdatableAppImage::_hasIsoMagicValue(std::ifstream& ifs) const {
        static constexpr int isoMagicPos = 32769;
        static const std::string isoMagicValue = "CD001";

        ifs.seekg(isoMagicPos);
        assertIfstreamGood(ifs);

        std::vector<char> isoMagicPosData(isoMagicValue.size() + 1, '\0');

        ifs.read(isoMagicPosData.data(), isoMagicValue.size());
        assertIfstreamGood(ifs);

        return isoMagicPosData.data() == isoMagicValue;
    }

    UpdatableAppImage::UpdatableAppImage(std::string path) : _path(std::move(path)) {}

    std::string UpdatableAppImage::path() const {
        return _path;
    }

    int UpdatableAppImage::appImageType() const {
        auto ifs = _open();

        // read magic number
        ifs.seekg(8, std::ios::beg);
        assertIfstreamGood(ifs);

        std::vector<char> magicByte(4, '\0');

        ifs.read(magicByte.data(), 3);
        assertIfstreamGood(ifs);

        // validate first two bytes are A and I
        if (magicByte[0] != 'A' && magicByte[1] != 'I') {
            std::ostringstream oss;
            oss << "Invalid magic bytes: " << (int) magicByte[0] << (int) magicByte[1];
            throw AppImageError(oss.str());
        }

        // for types 1 and 2, the third byte contains the type ID
        auto appImageType = magicByte[2];

        if (appImageType >= 1 && appImageType <= 2) {
            return appImageType;
        }

        // final try: type 1 AppImages do not have to set the magic bytes, although they should
        // if the file is both an ELF and an ISO9660 file, we'll suspect it to be a type 1 AppImage, and
        // proceed with a warning
        if (_hasElfMagicValue(ifs) && _hasIsoMagicValue(ifs)) {
            return 1;
        }

        throw AppImageError("Unknown AppImage type or not an AppImage");
    }

    std::string UpdatableAppImage::readSignature() const {
        const auto type = appImageType();

        if (type != 2) {
            throw AppImageError("Signature reading is not supported for type " + std::to_string(type));
        }

        return readElfSection(_path, ".sha256_sig");
    }

    std::string UpdatableAppImage::readSigningKey() const {
        const auto type = appImageType();

        if (type != 2) {
            throw AppImageError("Reading signing key is not supported for type " + std::to_string(type));
        }

        return readElfSection(_path, ".sig_key");
    }

    std::string UpdatableAppImage::readRawUpdateInformation() const {
        auto ifs = _open();

        int type;
        try {
            type = appImageType();
        } catch (const AppImageError& e) {
            // in case the ISO magic bytes can be found, we treat this file like a type 1 AppImage in this
            // special case
            // this is legacy behavior adopted from the current AppImageUpdate's predecessor
            if (_hasIsoMagicValue(ifs)) {
                type = 1;
            } else {
                std::rethrow_exception(std::current_exception());
            }
        }

        if (type == 1) {
            // update information is always at the same position, and has a fixed length
            static constexpr auto position = 0x8373;
            static constexpr auto length = 512;

            ifs.seekg(position);

            std::vector<char> rawUpdateInformation(length, 0);
            ifs.read(rawUpdateInformation.data(), length);

            return rawUpdateInformation.data();
        }

        if (type == 2) {
            // try to read ELF section .upd_info
            return readElfSection(_path, ".upd_info");
        }

        // should be unreachable
        throw AppImageError("Reading update information not supported for type " + std::to_string(type));
    }

    std::string UpdatableAppImage::calculateHash() const {
        // read offset and length of signature section to skip it later
        unsigned long sigOffset = 0, sigLength = 0;
        unsigned long keyOffset = 0, keyLength = 0;

        if (!appimage_get_elf_section_offset_and_length(_path.c_str(), ".sha256_sig", &sigOffset, &sigLength)) {
            throw AppImageError("Could not find .sha256_sig section in AppImage");
        }

        if (!appimage_get_elf_section_offset_and_length(_path.c_str(), ".sig_key", &keyOffset, &keyLength)) {
            throw AppImageError("Could not find .sig_key section in AppImage");
        }

        auto ifs = _open();

        ZSyncHash<GCRY_MD_SHA256> digest;

        // validate.c uses "offset" as chunk size, but that value might be quite high, and therefore uses
        // a lot of memory
        // TODO: use a smaller value (maybe use a prime factorization and use the biggest prime factor?)
        const std::streamsize chunkSize = 4096;

        std::vector<char> buffer(chunkSize, 0);

        ssize_t totalBytesRead = 0;

        // bytes that should be skipped when reading the next chunk
        // when e.g., a section that must be ignored spans over more than one chunk, this amount of bytes is
        // being nulled & skipped before reading data from the file again
        std::streamsize bytesToSkip = 0;

        ifs.seekg(0, std::ios::end);
        assertIfstreamGood(ifs);
        const ssize_t fileSize = ifs.tellg();
        assertIfstreamGood(ifs);
        ifs.seekg(0, std::ios::beg);
        assertIfstreamGood(ifs);

        while (ifs) {
            // in case we read less bytes than the chunk size, we resize the vector to the number of bytes read
            // we have to reset this upon the next iteration
            // this should be a no-op when the size has not changed, and should not be necessary, as the only time
            // we receive less bytes than requested should be the final read call
            buffer.resize(chunkSize);

            std::streamsize bytesRead = 0;

            auto bytesLeftInChunk = std::min(chunkSize, (fileSize - totalBytesRead));

            if (bytesLeftInChunk <= 0)
                break;

            auto skipBytes = [&bytesRead, &bytesLeftInChunk, &buffer, &ifs, &totalBytesRead, this](ssize_t count) {
                if (count <= 0)
                    return;

                const auto start = buffer.begin() + static_cast<long>(bytesRead);
                std::fill_n(start, count, '\0');

                bytesRead += count;
                totalBytesRead += count;
                bytesLeftInChunk -= count;

                ifs.seekg(count, std::ios::cur);
                assertIfstreamGood(ifs);
            };

            auto readBytes = [&bytesRead, &bytesLeftInChunk, &buffer, &ifs, &totalBytesRead, this](ssize_t count) {
                if (count <= 0)
                    return;

                ifs.read(buffer.data() + bytesRead, count);

                bytesRead += ifs.gcount();
                assertIfstreamGood(ifs);

                totalBytesRead += count;
                bytesLeftInChunk -= bytesRead;
            };

            auto checkSkipSection = [&](const std::streamsize sectionOffset, const std::streamsize sectionLength) {
                // check whether signature starts in current chunk
                const auto sectionOffsetDelta = sectionOffset - totalBytesRead;

                if (sectionOffsetDelta >= 0 && sectionOffsetDelta < bytesLeftInChunk) {
                    // read until section begins
                    readBytes(sectionOffsetDelta);

                    // calculate how many bytes must be nulled in this chunk
                    // the rest will be nulled in the following chunk(s)
                    auto bytesLeft = sectionLength;
                    const auto bytesToNullInCurrentChunk = std::min(bytesLeftInChunk, bytesLeft);

                    // null these bytes
                    skipBytes(bytesToNullInCurrentChunk);

                    // calculate how many bytes must be nulled in future chunks
                    bytesLeft -= bytesToNullInCurrentChunk;
                    bytesToSkip = bytesLeft;
                }
            };

            // check whether bytes must be skipped from previous sections
            if (bytesToSkip > 0) {
                auto bytesToSkipInCurrentChunk = std::min(chunkSize, bytesToSkip);
                skipBytes(bytesToSkipInCurrentChunk);
                bytesToSkip -= bytesToSkipInCurrentChunk;
            }

            // check whether one of the sections that must be skipped are in the current chunk, and if they
            // are, skip those sections in the current and future sections
            checkSkipSection(static_cast<std::streamsize>(sigOffset), static_cast<std::streamsize>(sigLength));
            checkSkipSection(static_cast<std::streamsize>(keyOffset), static_cast<std::streamsize>(keyLength));

            // read remaining bytes in chunk, given the file has still data to be read
            if (ifs && bytesLeftInChunk > 0) {
                readBytes(bytesLeftInChunk);
            }

            // make sure the buffer has the right size
            buffer.resize(bytesRead);

            // update hash with data from buffer
            digest.add(buffer);
        }

        return digest.getHash();
    }
}
