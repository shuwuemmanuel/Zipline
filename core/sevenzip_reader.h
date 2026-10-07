/* sevenzip_reader.h - read .7z archives, including AES-256 encrypted ones.
 *
 * libarchive can read plain .7z but cannot decrypt AES-encrypted content, so
 * this compact reader handles the structures 7-Zip / py7zr / Zipline produce:
 * plain or LZMA(2)-encoded headers, one or more folders using Copy / LZMA /
 * LZMA2 coders with an optional trailing AES-256 coder, and multiple files.
 * It does not handle header encryption (-mhe) or exotic BCJ chains; callers
 * fall back to libarchive for those. */
#ifndef ZIPLINE_SEVENZIP_READER_H
#define ZIPLINE_SEVENZIP_READER_H

#include "archive_manager.h"
#include <string>
#include <vector>
#include <cstdint>

namespace zipline {

struct SzReadResult {
    bool ok = false;
    bool unsupported = false;      /* structure we don't handle -> fall back */
    std::string error;
    std::vector<ArchiveEntry> entries;
    std::vector<std::vector<uint8_t>> data; /* parallel to entries, if withData */
};

/* Parse a .7z file.  If withData, also decode every file's bytes. */
SzReadResult sevenzipRead(const std::string &path,
                          const std::string &password,
                          bool withData);

} // namespace zipline

#endif
