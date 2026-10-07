#include "sevenzip_reader.h"

extern "C" {
#include "../sfx/sfx_codec.h"
#include "../third_party/lzma/LzmaDec.h"
#include "../third_party/lzma/Lzma2Dec.h"
#include "../third_party/lzma/Alloc.h"
}

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

namespace zipline {

namespace {

/* property ids (match the writer) */
enum {
    kEnd = 0, kHeader = 1, kArchiveProperties = 2, kAdditionalStreamsInfo = 3,
    kMainStreamsInfo = 4, kFilesInfo = 5, kPackInfo = 6, kUnpackInfo = 7,
    kSubStreamsInfo = 8, kSize = 9, kCRC = 10, kFolder = 11,
    kCodersUnpackSize = 12, kNumUnpackStream = 13, kEmptyStream = 14,
    kEmptyFile = 15, kAnti = 16, kName = 17, kCTime = 18, kATime = 19,
    kMTime = 20, kWinAttributes = 21, kComment = 22, kEncodedHeader = 23,
    kStartPos = 24, kDummy = 25
};

constexpr uint64_t METHOD_COPY  = 0x00;
constexpr uint64_t METHOD_LZMA  = 0x030101;
constexpr uint64_t METHOD_LZMA2 = 0x21;
constexpr uint64_t METHOD_AES   = 0x06F10701;

struct ParseError { std::string msg; bool unsupported; };

struct Reader {
    const uint8_t *p, *end;
    uint8_t u8() {
        if (p >= end) throw ParseError{"truncated header", false};
        return *p++;
    }
    uint64_t num() {
        uint8_t first = u8();
        uint64_t value = 0;
        uint8_t mask = 0x80;
        for (int i = 0; i < 8; i++) {
            if ((first & mask) == 0) {
                value |= ((uint64_t)(first & (mask - 1)) << (8 * i));
                return value;
            }
            value |= ((uint64_t)u8() << (8 * i));
            mask >>= 1;
        }
        return value;
    }
    void skip(uint64_t n) { if (p + n > end) throw ParseError{"truncated", false}; p += n; }
    void bytes(void *d, size_t n) { if (p + n > end) throw ParseError{"truncated", false}; memcpy(d, p, n); p += n; }
};

struct Coder {
    uint64_t method = 0;
    int numIn = 1, numOut = 1;
    std::vector<uint8_t> props;
};

struct Folder {
    std::vector<Coder> coders;
    std::vector<std::pair<uint64_t,uint64_t>> bindPairs; /* inIndex,outIndex */
    std::vector<uint64_t> packedIndices;                 /* in-stream indices from file */
    std::vector<uint64_t> unpackSizes;                   /* per out-stream */
    int numPackStreams = 1;

    int totalOut() const { int n=0; for (auto&c:coders) n+=c.numOut; return n; }
    int totalIn()  const { int n=0; for (auto&c:coders) n+=c.numIn;  return n; }

    /* the folder's final output stream = the out-stream not used in any bindpair */
    int finalOutIndex() const {
        int tot = totalOut();
        for (int o = 0; o < tot; ++o) {
            bool bound = false;
            for (auto &bp : bindPairs) if ((int)bp.second == o) bound = true;
            if (!bound) return o;
        }
        return 0;
    }
    uint64_t outputSize() const { return unpackSizes[finalOutIndex()]; }
};

struct StreamsInfo {
    uint64_t packPos = 0;
    std::vector<uint64_t> packSizes;
    std::vector<Folder> folders;
    /* substreams */
    std::vector<uint64_t> numUnpack;     /* per folder */
    std::vector<uint64_t> subSizes;      /* per substream (flattened) */
    std::vector<uint32_t> subCrcs;
};

void readPackInfo(Reader &r, StreamsInfo &si)
{
    si.packPos = r.num();
    uint64_t n = r.num();
    for (;;) {
        uint8_t id = r.u8();
        if (id == kEnd) break;
        if (id == kSize) {
            for (uint64_t i = 0; i < n; ++i) si.packSizes.push_back(r.num());
        } else if (id == kCRC) {
            uint8_t all = r.u8();
            uint64_t defined = all ? n : 0;
            if (!all) { /* bit vector */ defined = 0; uint8_t mask=0; uint8_t b=0;
                for (uint64_t i=0;i<n;i++){ if(mask==0){b=r.u8();mask=0x80;} if(b&mask)defined++; mask>>=1;} }
            for (uint64_t i = 0; i < defined; ++i) r.skip(4);
        } else {
            throw ParseError{"unknown PackInfo id", true};
        }
    }
}

Folder readFolder(Reader &r)
{
    Folder f;
    uint64_t numCoders = r.num();
    for (uint64_t i = 0; i < numCoders; ++i) {
        uint8_t flag = r.u8();
        int idSize = flag & 0x0F;
        Coder c;
        uint64_t method = 0;
        for (int k = 0; k < idSize; ++k) method = (method << 8) | r.u8();
        c.method = method;
        if (flag & 0x10) { c.numIn = (int)r.num(); c.numOut = (int)r.num(); }
        else { c.numIn = 1; c.numOut = 1; }
        if (flag & 0x20) { uint64_t ps = r.num(); c.props.resize(ps); r.bytes(c.props.data(), ps); }
        if (flag & 0x80) throw ParseError{"alternative methods unsupported", true};
        f.coders.push_back(c);
    }
    int numBindPairs = f.totalOut() - 1;
    for (int i = 0; i < numBindPairs; ++i) {
        uint64_t in = r.num(), out = r.num();
        f.bindPairs.push_back({in, out});
    }
    int numPack = f.totalIn() - numBindPairs;
    f.numPackStreams = numPack;
    if (numPack == 1) {
        /* implicit: the single unbound in-stream */
        for (int i = 0; i < f.totalIn(); ++i) {
            bool bound = false;
            for (auto &bp : f.bindPairs) if ((int)bp.first == i) bound = true;
            if (!bound) { f.packedIndices.push_back(i); break; }
        }
    } else {
        for (int i = 0; i < numPack; ++i) f.packedIndices.push_back(r.num());
    }
    return f;
}

void readUnpackInfo(Reader &r, StreamsInfo &si)
{
    uint8_t id = r.u8();
    if (id != kFolder) throw ParseError{"expected kFolder", true};
    uint64_t numFolders = r.num();
    uint8_t external = r.u8();
    if (external) throw ParseError{"external folders unsupported", true};
    for (uint64_t i = 0; i < numFolders; ++i)
        si.folders.push_back(readFolder(r));

    id = r.u8();
    if (id != kCodersUnpackSize) throw ParseError{"expected CodersUnpackSize", true};
    for (auto &f : si.folders)
        for (int o = 0; o < f.totalOut(); ++o)
            f.unpackSizes.push_back(r.num());

    for (;;) {
        id = r.u8();
        if (id == kEnd) break;
        if (id == kCRC) {
            uint64_t n = si.folders.size();
            uint8_t all = r.u8();
            uint64_t defined = all ? n : 0;
            if (!all){ defined=0; uint8_t mask=0,b=0; for(uint64_t i=0;i<n;i++){if(mask==0){b=r.u8();mask=0x80;}if(b&mask)defined++;mask>>=1;} }
            for (uint64_t i = 0; i < defined; ++i) r.skip(4);
        } else {
            throw ParseError{"unknown UnpackInfo id", true};
        }
    }
}

void readSubStreamsInfo(Reader &r, StreamsInfo &si)
{
    uint8_t id = r.u8();
    if (id == kNumUnpackStream) {
        for (size_t i = 0; i < si.folders.size(); ++i) si.numUnpack.push_back(r.num());
        id = r.u8();
    } else {
        for (size_t i = 0; i < si.folders.size(); ++i) si.numUnpack.push_back(1);
    }

    /* sizes */
    for (size_t fi = 0; fi < si.folders.size(); ++fi) {
        uint64_t ns = si.numUnpack[fi];
        if (ns == 0) continue;
        uint64_t sum = 0;
        for (uint64_t i = 0; i + 1 < ns; ++i) {
            uint64_t sz = 0;
            if (id == kSize) sz = r.num();
            si.subSizes.push_back(sz);
            sum += sz;
        }
        si.subSizes.push_back(si.folders[fi].outputSize() - sum); /* last implicit */
    }
    if (id == kSize) id = r.u8();

    /* crcs */
    for (;;) {
        if (id == kEnd) break;
        if (id == kCRC) {
            uint64_t total = 0;
            for (auto n : si.numUnpack) total += n;
            uint8_t all = r.u8();
            std::vector<bool> defined(total, all != 0);
            if (!all) { uint8_t mask=0,b=0; for(uint64_t i=0;i<total;i++){if(mask==0){b=r.u8();mask=0x80;}defined[i]=(b&mask);mask>>=1;} }
            for (uint64_t i = 0; i < total; ++i) {
                uint32_t crc = 0;
                if (defined[i]) { uint8_t t[4]; r.bytes(t,4); crc = t[0]|(t[1]<<8)|(t[2]<<16)|((uint32_t)t[3]<<24); }
                si.subCrcs.push_back(crc);
            }
            id = r.u8();
        } else {
            /* skip unknown */
            throw ParseError{"unknown SubStreamsInfo id", true};
        }
    }
}

void readStreamsInfo(Reader &r, StreamsInfo &si)
{
    for (;;) {
        uint8_t id = r.u8();
        if (id == kEnd) break;
        else if (id == kPackInfo) readPackInfo(r, si);
        else if (id == kUnpackInfo) readUnpackInfo(r, si);
        else if (id == kSubStreamsInfo) readSubStreamsInfo(r, si);
        else throw ParseError{"unknown StreamsInfo id", true};
    }
    if (si.numUnpack.empty())
        for (size_t i = 0; i < si.folders.size(); ++i) si.numUnpack.push_back(1);
    if (si.subSizes.empty())
        for (auto &f : si.folders) si.subSizes.push_back(f.outputSize());
}

/* ---- decoders ---- */

std::vector<uint8_t> lzmaDecode(const std::vector<uint8_t> &props,
                                const uint8_t *in, size_t inLen, size_t outLen)
{
    std::vector<uint8_t> out(outLen ? outLen : 1);
    CLzmaDec dec;
    LzmaDec_Construct(&dec);
    if (LzmaDec_Allocate(&dec, props.data(), (unsigned)props.size(), &g_Alloc) != SZ_OK)
        throw ParseError{"lzma alloc", false};
    LzmaDec_Init(&dec);
    SizeT dstLen = outLen, srcLen = inLen;
    ELzmaStatus status;
    SRes r = LzmaDec_DecodeToBuf(&dec, out.data(), &dstLen, in, &srcLen,
                                 LZMA_FINISH_END, &status);
    LzmaDec_Free(&dec, &g_Alloc);
    if (r != SZ_OK || dstLen != outLen) throw ParseError{"lzma decode failed", false};
    return out;
}

std::vector<uint8_t> lzma2Decode(const std::vector<uint8_t> &props,
                                 const uint8_t *in, size_t inLen, size_t outLen)
{
    std::vector<uint8_t> out(outLen ? outLen : 1);
    if (props.empty()) throw ParseError{"lzma2 missing props", false};
    if (sfx_lzma2_decompress(props[0], in, inLen, out.data(), outLen) != 0)
        throw ParseError{"lzma2 decode failed", false};
    return out;
}

/* Execute a folder's coder chain: 'packed' are the folder's packed streams
 * concatenated (we only support a single packed stream). */
std::vector<uint8_t> decodeFolder(const Folder &f, std::vector<uint8_t> packed,
                                  const std::string &password)
{
    /* Identify coders. Supported chains:
     *   [LZMA/LZMA2]                      (packed -> coder -> output)
     *   [LZMA/LZMA2] + [AES]  with AES decrypting the packed stream first. */
    const Coder *aes = nullptr, *lz = nullptr;
    for (auto &c : f.coders) {
        if (c.method == METHOD_AES) aes = &c;
        else if (c.method == METHOD_LZMA || c.method == METHOD_LZMA2 || c.method == METHOD_COPY) lz = &c;
        else throw ParseError{"unsupported coder", true};
    }
    if (!lz) throw ParseError{"no main coder", true};

    std::vector<uint8_t> comp;
    if (aes) {
        /* parse AES props: numCyclesPower + salt + iv */
        const auto &pr = aes->props;
        if (pr.size() < 1) throw ParseError{"bad AES props", false};
        int power = pr[0] & 0x3F;
        int saltSize = 0, ivSize = 0;
        size_t pos = 1;
        if (pr[0] & 0xC0) {
            if (pr.size() < 2) throw ParseError{"bad AES props", false};
            uint8_t b = pr[1]; pos = 2;
            saltSize = ((pr[0] >> 7) & 1) + (b >> 4);
            ivSize = ((pr[0] >> 6) & 1) + (b & 0x0F);
        }
        std::vector<uint8_t> salt(pr.begin()+pos, pr.begin()+pos+saltSize);
        std::vector<uint8_t> iv(16, 0);
        for (int i = 0; i < ivSize && i < 16; ++i) iv[i] = pr[pos+saltSize+i];
        uint8_t key[32];
        sfx_derive_key_ex(password.c_str(), salt.data(), salt.size(), power, key);
        if (packed.size() % 16 != 0) throw ParseError{"bad ciphertext size", false};
        sfx_aes_cbc_decrypt(key, iv.data(), packed.data(), packed.size());
        memset(key, 0, sizeof(key));
        comp = std::move(packed);
    } else {
        comp = std::move(packed);
    }

    /* output size of the main coder = its out-stream unpack size.
     * Find the global out-stream index of lz's output. */
    int outIdx = 0;
    for (auto &c : f.coders) { if (&c == lz) break; outIdx += c.numOut; }
    uint64_t outSize = f.unpackSizes[outIdx];

    if (lz->method == METHOD_COPY) {
        comp.resize(outSize);
        return comp;
    }
    if (lz->method == METHOD_LZMA)
        return lzmaDecode(lz->props, comp.data(), comp.size(), outSize);
    return lzma2Decode(lz->props, comp.data(), comp.size(), outSize);
}

} // namespace

SzReadResult sevenzipRead(const std::string &path, const std::string &password, bool withData)
{
    SzReadResult res;
    try {
        std::ifstream f(path, std::ios::binary);
        if (!f) { res.error = "cannot open file"; return res; }
        std::vector<uint8_t> all((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
        if (all.size() < 32 || memcmp(all.data(), "7z\xBC\xAF\x27\x1C", 6) != 0) {
            res.unsupported = true; res.error = "not a 7z file"; return res;
        }
        auto rd64 = [&](size_t off) {
            uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= (uint64_t)all[off+i] << (8*i); return v;
        };
        uint64_t nextOff = rd64(12), nextSize = rd64(20);
        size_t base = 32;
        if (base + nextOff + nextSize > all.size()) { res.error = "corrupt 7z"; return res; }

        std::vector<uint8_t> headerBytes(all.begin()+base+nextOff,
                                         all.begin()+base+nextOff+nextSize);
        Reader r{headerBytes.data(), headerBytes.data()+headerBytes.size()};

        uint8_t id = r.u8();
        if (id == kEncodedHeader) {
            /* header is itself a compressed stream; decode it */
            StreamsInfo hsi;
            readStreamsInfo(r, hsi);
            if (hsi.folders.size() != 1) throw ParseError{"multi-folder header", true};
            size_t packStart = base + hsi.packPos;
            uint64_t packLen = hsi.packSizes.empty() ? 0 : hsi.packSizes[0];
            std::vector<uint8_t> packed(all.begin()+packStart, all.begin()+packStart+packLen);
            std::vector<uint8_t> decoded = decodeFolder(hsi.folders[0], std::move(packed), password);
            headerBytes = std::move(decoded);
            r = Reader{headerBytes.data(), headerBytes.data()+headerBytes.size()};
            id = r.u8();
        }
        if (id != kHeader) throw ParseError{"expected header", true};

        StreamsInfo si;
        std::vector<std::string> names;
        std::vector<bool> emptyStream, emptyFile;
        uint64_t numFiles = 0;

        for (;;) {
            uint8_t pid = r.u8();
            if (pid == kEnd) break;
            if (pid == kMainStreamsInfo) {
                readStreamsInfo(r, si);
            } else if (pid == kFilesInfo) {
                numFiles = r.num();
                emptyStream.assign(numFiles, false);
                emptyFile.clear();
                for (;;) {
                    uint8_t prop = r.u8();
                    if (prop == kEnd) break;
                    uint64_t sz = r.num();
                    const uint8_t *propEnd = r.p + sz;
                    if (prop == kEmptyStream) {
                        uint8_t mask=0,b=0;
                        for (uint64_t i=0;i<numFiles;i++){ if(mask==0){b=r.u8();mask=0x80;} emptyStream[i]=(b&mask); mask>>=1; }
                    } else if (prop == kEmptyFile) {
                        /* bit vector over empty streams; ignored (treat as empty files) */
                        r.p = propEnd;
                    } else if (prop == kName) {
                        uint8_t external = r.u8();
                        if (external) throw ParseError{"external names", true};
                        /* UTF-16LE names, NUL-terminated */
                        std::string cur;
                        while (r.p + 1 < propEnd) {
                            uint16_t w = r.u8(); w |= (uint16_t)r.u8() << 8;
                            if (w == 0) { names.push_back(cur); cur.clear(); }
                            else {
                                /* basic BMP to UTF-8 */
                                if (w == '\\') cur += '/';
                                else if (w < 0x80) cur += (char)w;
                                else if (w < 0x800) { cur += (char)(0xC0|(w>>6)); cur += (char)(0x80|(w&0x3F)); }
                                else { cur += (char)(0xE0|(w>>12)); cur += (char)(0x80|((w>>6)&0x3F)); cur += (char)(0x80|(w&0x3F)); }
                            }
                        }
                        r.p = propEnd;
                    } else {
                        r.p = propEnd; /* skip times, attrs, etc. */
                    }
                }
            } else {
                throw ParseError{"unknown header prop", true};
            }
        }

        /* Decode folders (if needed) and split into substreams. */
        size_t dataBase = base + si.packPos;
        std::vector<std::vector<uint8_t>> folderData;
        std::vector<size_t> packOffsets;
        {
            size_t off = dataBase;
            size_t packIdx = 0;
            for (size_t fi = 0; fi < si.folders.size(); ++fi) {
                uint64_t packLen = 0;
                for (int k = 0; k < si.folders[fi].numPackStreams; ++k)
                    packLen += (packIdx < si.packSizes.size()) ? si.packSizes[packIdx++] : 0;
                if (withData) {
                    std::vector<uint8_t> packed(all.begin()+off, all.begin()+off+packLen);
                    folderData.push_back(decodeFolder(si.folders[fi], std::move(packed), password));
                } else {
                    folderData.push_back({});
                }
                off += packLen;
            }
        }

        /* Map substreams to files (accounting for empty-stream entries). */
        size_t subIdx = 0, folderIdx = 0, inFolder = 0;
        size_t folderPos = 0;
        for (uint64_t fi = 0; fi < numFiles; ++fi) {
            ArchiveEntry e;
            e.name = (fi < names.size()) ? names[fi] : ("file" + std::to_string(fi));
            auto slash = e.name.find_last_of('/');
            e.path = slash == std::string::npos ? "" : e.name.substr(0, slash);
            {
                auto dot = e.name.find_last_of('.');
                e.type = (dot == std::string::npos) ? "File" : e.name.substr(dot+1);
                for (auto &c : e.type) c = (char)toupper((unsigned char)c);
            }
            if (fi < emptyStream.size() && emptyStream[fi]) {
                e.size = 0;
                res.entries.push_back(e);
                if (withData) res.data.push_back({});
                continue;
            }
            uint64_t sz = (subIdx < si.subSizes.size()) ? si.subSizes[subIdx] : 0;
            e.size = sz;
            res.entries.push_back(e);
            if (withData) {
                std::vector<uint8_t> d;
                if (folderIdx < folderData.size()) {
                    auto &fd = folderData[folderIdx];
                    if (folderPos + sz <= fd.size())
                        d.assign(fd.begin()+folderPos, fd.begin()+folderPos+sz);
                }
                if (subIdx < si.subCrcs.size() && si.subCrcs[subIdx] != 0) {
                    uint32_t got = sfx_crc32(d.data(), d.size());
                    if (got != si.subCrcs[subIdx])
                        throw ParseError{"CRC mismatch (wrong password or corrupt archive)", false};
                }
                res.data.push_back(std::move(d));
            }
            folderPos += sz;
            subIdx++;
            if (++inFolder >= (folderIdx < si.numUnpack.size() ? si.numUnpack[folderIdx] : 1)) {
                folderIdx++; inFolder = 0; folderPos = 0;
            }
        }

        res.ok = true;
        return res;
    } catch (ParseError &e) {
        res.error = e.msg;
        res.unsupported = e.unsupported;
        return res;
    } catch (std::exception &e) {
        res.error = e.what();
        return res;
    }
}

} // namespace zipline
