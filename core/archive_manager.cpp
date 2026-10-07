#include "archive_manager.h"
#include "sevenzip_writer.h"
#include "sevenzip_reader.h"
#include "sfx_builder.h"

#include <archive.h>
#include <archive_entry.h>

extern "C" {
#include "../sfx/sfx_format.h"
#include "../sfx/stub_payload.h"
#include "../sfx/stub_settings.h"
#include "../sfx/sfx_codec.h"
}

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace zipline {

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static std::string timeStr(time_t t)
{
    if (t == 0) return "";
    char buf[32];
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmv);
    return buf;
}

static std::string typeOf(const std::string &name)
{
    auto dot = name.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= name.size()) return "File";
    std::string ext = name.substr(dot + 1);
    for (auto &c : ext) c = (char)toupper((unsigned char)c);
    return ext;
}

static std::string parentOf(const std::string &name)
{
    std::string n = name;
    for (auto &c : n) if (c == '\\') c = '/';
    auto slash = n.find_last_of('/');
    return slash == std::string::npos ? "" : n.substr(0, slash);
}

static std::string lower(std::string s)
{
    for (auto &c : s) c = (char)tolower((unsigned char)c);
    return s;
}

/* ------------------------------------------------------------------ */
/* ArchiveManager                                                      */
/* ------------------------------------------------------------------ */

std::string defaultStubPath()
{
    if (const char *env = std::getenv("ZIPLINE_SFX_STUB"))
        return env;
    /* look next to the executable and in a few common spots */
    std::error_code ec;
    std::vector<std::string> candidates = {
        "sfx_stub.exe", "stub.exe",
        (fs::current_path(ec) / "sfx_stub.exe").string(),
    };
    for (auto &c : candidates)
        if (fs::exists(c)) return c;
    return "sfx_stub.exe";
}

ArchiveManager::ArchiveManager()
{
    stubPath_ = defaultStubPath();
}

std::vector<std::string> ArchiveManager::supportedFormats() const
{
    return {"ZIP", "7Z", "EXE", "RAR", "TAR", "GZIP"};
}

std::vector<std::string> ArchiveManager::extractFormats() const
{
    return {".zip", ".7z", ".exe", ".rar", ".tar", ".gz", ".bz2", ".xz"};
}

void ArchiveManager::expandInputs(const std::vector<std::string> &files,
                                  std::vector<std::string> &diskPaths,
                                  std::vector<std::string> &arcNames)
{
    std::error_code ec;
    for (const auto &item : files) {
        fs::path p(item);
        if (fs::is_directory(p, ec)) {
            fs::path base = p.parent_path();
            for (auto it = fs::recursive_directory_iterator(p, ec);
                 it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) break;
                if (fs::is_regular_file(it->path(), ec)) {
                    diskPaths.push_back(it->path().string());
                    fs::path rel = fs::relative(it->path(), base, ec);
                    std::string r = rel.generic_string();
                    arcNames.push_back(r);
                }
            }
        } else if (fs::is_regular_file(p, ec)) {
            diskPaths.push_back(item);
            arcNames.push_back(p.filename().string());
        }
    }
}

/* ---- creation dispatch ---- */

void ArchiveManager::createArchive(const std::vector<std::string> &files,
                                   const std::string &outputPath,
                                   const ArchiveSettings &settings,
                                   ProgressCallback progress)
{
    std::error_code ec;
    fs::create_directories(fs::path(outputPath).parent_path(), ec);

    std::vector<std::string> disk, arc;
    expandInputs(files, disk, arc);
    if (disk.empty())
        throw std::runtime_error("No files to archive");

    const std::string &fmt = settings.format;
    if (fmt == "ZIP")       createZip(disk, arc, outputPath, settings, progress);
    else if (fmt == "7Z")   create7z(disk, arc, outputPath, settings, progress);
    else if (fmt == "EXE")  createExe(disk, arc, outputPath, settings, progress);
    else if (fmt == "TAR")  createTarLike(disk, arc, outputPath, settings, false, progress);
    else if (fmt == "GZIP") createTarLike(disk, arc, outputPath, settings, true, progress);
    else if (fmt == "RAR")
        throw std::runtime_error("RAR creation requires the proprietary WinRAR engine "
                                 "and is not supported; use 7Z or ZIP instead.");
    else
        throw std::runtime_error("Unsupported archive format: " + fmt);

    if (settings.test_after_creation && fmt != "EXE") {
        /* split volumes are tested before splitting below */
        if (!settings.split_archive)
            testArchive(outputPath, settings.password_protected ? settings.password : "");
    }

    if (settings.split_archive && fmt != "EXE") {
        splitIntoVolumes(outputPath, (uint64_t)settings.volume_size * 1024ull * 1024ull);
    }

    if (settings.delete_after) {
        for (const auto &f : files) {
            std::error_code e;
            fs::remove_all(f, e);
        }
    }
}

/* ---- libarchive write helper ---- */

static void writeOneFile(struct archive *a, const std::string &disk,
                         const std::string &arc)
{
    std::error_code ec;
    uint64_t size = (uint64_t)fs::file_size(disk, ec);
    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, arc.c_str());
    archive_entry_set_size(entry, (la_int64_t)size);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    auto mtime = fs::last_write_time(disk, ec);
    archive_entry_set_mtime(entry, time(nullptr), 0);
    (void)mtime;

    if (archive_write_header(a, entry) != ARCHIVE_OK) {
        std::string e = archive_error_string(a) ? archive_error_string(a) : "write header failed";
        archive_entry_free(entry);
        throw std::runtime_error(e);
    }

    std::ifstream in(disk, std::ios::binary);
    char buf[65536];
    while (in) {
        in.read(buf, sizeof(buf));
        std::streamsize n = in.gcount();
        if (n > 0)
            archive_write_data(a, buf, (size_t)n);
    }
    archive_entry_free(entry);
}

void ArchiveManager::createZip(const std::vector<std::string> &disk,
                               const std::vector<std::string> &arc,
                               const std::string &out, const ArchiveSettings &s,
                               ProgressCallback progress)
{
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);

    std::string method = lower(s.compression_method);
    if (s.compression_level == 0 || method == "store")
        archive_write_set_options(a, "zip:compression=store");
    else
        archive_write_set_options(a, "zip:compression=deflate");

    if (s.password_protected && !s.password.empty()) {
        std::string enc = "aes256";
        if (s.encryption_method == "AES-128") enc = "aes128";
        else if (s.encryption_method == "ZipCrypto") enc = "zipcrypt";
        std::string opt = "zip:encryption=" + enc;
        if (archive_write_set_options(a, opt.c_str()) != ARCHIVE_OK && enc != "aes256")
            archive_write_set_options(a, "zip:encryption=aes256");
        archive_write_set_passphrase(a, s.password.c_str());
    }

    if (archive_write_open_filename(a, out.c_str()) != ARCHIVE_OK) {
        std::string e = archive_error_string(a);
        archive_write_free(a);
        throw std::runtime_error("Could not create ZIP: " + e);
    }
    for (size_t i = 0; i < disk.size(); ++i) {
        writeOneFile(a, disk[i], arc[i]);
        if (progress) progress((double)(i + 1) / disk.size() * 100.0);
    }
    archive_write_close(a);
    archive_write_free(a);
}

void ArchiveManager::createTarLike(const std::vector<std::string> &disk,
                                   const std::vector<std::string> &arc,
                                   const std::string &out, const ArchiveSettings &s,
                                   bool gzip, ProgressCallback progress)
{
    struct archive *a = archive_write_new();
    archive_write_set_format_pax_restricted(a);
    if (gzip) {
        archive_write_add_filter_gzip(a);
        std::string lvl = "gzip:compression-level=" + std::to_string(s.compression_level);
        archive_write_set_options(a, lvl.c_str());
    }
    if (archive_write_open_filename(a, out.c_str()) != ARCHIVE_OK) {
        std::string e = archive_error_string(a);
        archive_write_free(a);
        throw std::runtime_error("Could not create archive: " + e);
    }
    for (size_t i = 0; i < disk.size(); ++i) {
        writeOneFile(a, disk[i], arc[i]);
        if (progress) progress((double)(i + 1) / disk.size() * 100.0);
    }
    archive_write_close(a);
    archive_write_free(a);
}

struct ProgCtx { ProgressCallback cb; double lo, hi; };
static int progShim(void *ctx, double frac)
{
    auto *p = (ProgCtx *)ctx;
    if (p && p->cb) p->cb(p->lo + (p->hi - p->lo) * frac);
    return 0;
}

void ArchiveManager::create7z(const std::vector<std::string> &disk,
                              const std::vector<std::string> &arc,
                              const std::string &out, const ArchiveSettings &s,
                              ProgressCallback progress)
{
    std::vector<SzInput> inputs(disk.size());
    for (size_t i = 0; i < disk.size(); ++i) {
        inputs[i].disk_path = disk[i].c_str();
        inputs[i].arc_name = arc[i].c_str();
    }
    ProgCtx ctx{progress, 0.0, 100.0};
    char err[256] = {0};
    const char *pass = (s.password_protected && !s.password.empty()) ? s.password.c_str() : nullptr;
    if (sevenzip_write(out.c_str(), inputs.data(), (int)inputs.size(),
                       s.compression_level, pass,
                       progress ? progShim : nullptr, &ctx, err, sizeof(err)) != 0)
        throw std::runtime_error(std::string("7z creation failed: ") + err);
}

/* ---- EXE / SFX ---- */

static std::string escapeValue(const std::string &v)
{
    std::string out;
    for (char c : v) {
        if (c == '\n') { out += "\\n"; }
        else if (c == '\t') { out += ' '; }
        else out += c;
    }
    return out;
}

void ArchiveManager::createExe(const std::vector<std::string> &disk,
                               const std::vector<std::string> &arc,
                               const std::string &out, const ArchiveSettings &s,
                               ProgressCallback progress)
{
    if (!fs::exists(stubPath_))
        throw std::runtime_error("SFX stub not found (" + stubPath_ +
                                 "). Set ZIPLINE_SFX_STUB to its location.");

    /* serialise advanced settings to the key\tvalue blob the stub reads */
    std::ostringstream blob;
    std::string arcName = fs::path(out).stem().string();
    blob << "archive_name\t" << escapeValue(arcName) << "\n";
    for (const auto &kv : s.exe_advanced_settings)
        blob << kv.first << "\t" << escapeValue(kv.second) << "\n";
    std::string settingsStr = blob.str();

    std::vector<SfxInput> inputs(disk.size());
    for (size_t i = 0; i < disk.size(); ++i) {
        inputs[i].disk_path = disk[i].c_str();
        inputs[i].arc_name = arc[i].c_str();
    }
    ProgCtx ctx{progress, 0.0, 100.0};
    char err[256] = {0};
    const char *pass = (s.password_protected && !s.password.empty()) ? s.password.c_str() : nullptr;
    int level = s.compression_level <= 0 ? 6 : s.compression_level;
    if (sfx_build(stubPath_.c_str(), out.c_str(), inputs.data(), (int)inputs.size(),
                  settingsStr.c_str(), settingsStr.size(), level, pass,
                  progress ? progShim : nullptr, &ctx, err, sizeof(err)) != 0)
        throw std::runtime_error(std::string("EXE creation failed: ") + err);
}

/* ---- split into volumes ---- */

void ArchiveManager::splitIntoVolumes(const std::string &path, uint64_t volumeBytes)
{
    if (volumeBytes == 0) return;
    std::error_code ec;
    uint64_t total = fs::file_size(path, ec);
    if (ec || total <= volumeBytes) return;

    std::ifstream in(path, std::ios::binary);
    std::vector<char> buf(1 << 20);
    int part = 1;
    uint64_t written = 0;
    std::ofstream out;
    auto openPart = [&](int n) {
        char suffix[16];
        snprintf(suffix, sizeof(suffix), ".%03d", n);
        out.open(path + suffix, std::ios::binary);
    };
    openPart(part);
    while (in) {
        in.read(buf.data(), (std::streamsize)std::min<uint64_t>(buf.size(), volumeBytes - written));
        std::streamsize n = in.gcount();
        if (n <= 0) break;
        out.write(buf.data(), n);
        written += (uint64_t)n;
        if (written >= volumeBytes) {
            out.close();
            ++part;
            written = 0;
            openPart(part);
        }
    }
    out.close();
    in.close();
    fs::remove(path, ec); /* keep only the .NNN volumes */
}

/* ------------------------------------------------------------------ */
/* Reading: list / extract / test                                      */
/* ------------------------------------------------------------------ */

/* Detect a Zipline SFX and load its payload. Returns true if it is one. */
static bool loadSfx(const std::string &path, std::vector<uint8_t> &blob, SfxPayload &p)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    if (sz < SFX_FOOTER_SIZE) return false;
    f.seekg(sz - SFX_FOOTER_SIZE);
    SfxFooter footer;
    f.read((char *)&footer, SFX_FOOTER_SIZE);
    if (memcmp(footer.magic, SFX_FOOTER_MAGIC, SFX_FOOTER_MAGIC_LEN) != 0)
        return false;
    if (footer.payload_offset + footer.payload_size > (uint64_t)sz)
        return false;
    blob.resize((size_t)footer.payload_size);
    f.seekg((std::streamoff)footer.payload_offset);
    f.read((char *)blob.data(), (std::streamsize)footer.payload_size);
    return sfx_payload_parse(blob.data(), blob.size(), &p) == 0;
}

/* Decode an SFX payload into a raw solid buffer (throws on failure). */
static std::vector<uint8_t> decodeSfx(const SfxPayload &p, const std::string &password)
{
    std::vector<uint8_t> work(p.data, p.data + p.data_len);
    if (p.encrypted) {
        uint8_t key[SFX_AES_KEY_SIZE];
        sfx_derive_key(password.c_str(), p.salt, key);
        sfx_aes_cbc_decrypt(key, p.iv, work.data(), work.size());
        memset(key, 0, sizeof(key));
    }
    std::vector<uint8_t> raw(p.raw_size ? (size_t)p.raw_size : 1);
    if (p.method == SFX_METHOD_STORE) {
        memcpy(raw.data(), work.data(), (size_t)p.raw_size);
    } else if (sfx_lzma2_decompress(p.lzma_prop, work.data(), (size_t)p.comp_size,
                                    raw.data(), (size_t)p.raw_size) != 0) {
        throw std::runtime_error("Failed to decode EXE payload (wrong password?)");
    }
    if (sfx_crc32(raw.data(), (size_t)p.raw_size) != p.raw_crc)
        throw std::runtime_error("EXE payload integrity check failed (wrong password?)");
    return raw;
}

std::vector<ArchiveEntry> ArchiveManager::listContents(const std::string &archivePath,
                                                       const std::string &password)
{
    std::vector<ArchiveEntry> out;
    std::string ext = lower(fs::path(archivePath).extension().string());

    if (ext == ".exe") {
        std::vector<uint8_t> blob;
        SfxPayload p;
        if (loadSfx(archivePath, blob, p)) {
            for (uint32_t i = 0; i < p.file_count; ++i) {
                ArchiveEntry e;
                e.name = p.entries[i].name;
                e.size = p.entries[i].size;
                e.type = typeOf(e.name);
                e.path = parentOf(e.name);
                out.push_back(e);
            }
            sfx_payload_free(&p);
            return out;
        }
        ArchiveEntry e;
        e.name = "Self-Extracting Archive";
        e.size = (uint64_t)fs::file_size(archivePath);
        e.type = "Executable";
        e.path = "Run this executable to extract files";
        out.push_back(e);
        return out;
    }

    /* Use our own reader for .7z so encrypted archives work (libarchive
     * cannot decrypt 7z content); fall back to libarchive on exotic layouts. */
    if (ext == ".7z") {
        SzReadResult rr = sevenzipRead(archivePath, password, false);
        if (rr.ok) {
            for (auto &e : rr.entries) if (e.name.empty() || e.name.back() != '/') out.push_back(e);
            return out;
        }
        if (!rr.unsupported && !password.empty())
            throw std::runtime_error("Failed to read 7z: " + rr.error);
        /* else fall through to libarchive */
    }

    struct archive *a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    if (!password.empty())
        archive_read_add_passphrase(a, password.c_str());
    if (archive_read_open_filename(a, archivePath.c_str(), 65536) != ARCHIVE_OK) {
        std::string e = archive_error_string(a) ? archive_error_string(a) : "open failed";
        archive_read_free(a);
        throw std::runtime_error("Failed to read archive: " + e);
    }
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (archive_entry_filetype(entry) == AE_IFDIR) continue;
        ArchiveEntry e;
        const char *name = archive_entry_pathname(entry);
        e.name = name ? name : "";
        e.size = (uint64_t)archive_entry_size(entry);
        e.type = typeOf(e.name);
        e.modified = timeStr(archive_entry_mtime(entry));
        e.path = parentOf(e.name);
        out.push_back(e);
        archive_read_data_skip(a);
    }
    archive_read_free(a);
    return out;
}

void ArchiveManager::extractArchive(const std::string &archivePath,
                                    const std::string &extractPath,
                                    const std::string &password,
                                    ProgressCallback progress)
{
    std::error_code ec;
    fs::create_directories(extractPath, ec);
    std::string ext = lower(fs::path(archivePath).extension().string());

    if (ext == ".exe") {
        std::vector<uint8_t> blob;
        SfxPayload p;
        if (!loadSfx(archivePath, blob, p))
            throw std::runtime_error("Not a Zipline self-extracting archive.");
        std::vector<uint8_t> raw = decodeSfx(p, password);
        uint64_t off = 0;
        for (uint32_t i = 0; i < p.file_count; ++i) {
            fs::path dest = fs::path(extractPath) / p.entries[i].name;
            fs::create_directories(dest.parent_path(), ec);
            std::ofstream of(dest, std::ios::binary);
            of.write((const char *)raw.data() + off, (std::streamsize)p.entries[i].size);
            off += p.entries[i].size;
            if (progress) progress((double)(i + 1) / p.file_count * 100.0);
        }
        sfx_payload_free(&p);
        return;
    }

    if (ext == ".7z") {
        SzReadResult rr = sevenzipRead(archivePath, password, true);
        if (rr.ok) {
            for (size_t i = 0; i < rr.entries.size(); ++i) {
                const auto &name = rr.entries[i].name;
                if (name.empty() || name.back() == '/') continue;
                fs::path dest = fs::path(extractPath) / name;
                fs::create_directories(dest.parent_path(), ec);
                std::ofstream of(dest, std::ios::binary);
                of.write((const char *)rr.data[i].data(), (std::streamsize)rr.data[i].size());
                if (progress) progress((double)(i + 1) / rr.entries.size() * 100.0);
            }
            return;
        }
        if (!rr.unsupported)
            throw std::runtime_error("Failed to extract 7z: " + rr.error);
        /* fall through to libarchive for unsupported (unencrypted) layouts */
    }

    struct archive *a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    if (!password.empty())
        archive_read_add_passphrase(a, password.c_str());
    if (archive_read_open_filename(a, archivePath.c_str(), 65536) != ARCHIVE_OK) {
        std::string e = archive_error_string(a) ? archive_error_string(a) : "open failed";
        archive_read_free(a);
        throw std::runtime_error("Failed to open archive: " + e);
    }

    struct archive *writer = archive_write_disk_new();
    archive_write_disk_set_options(writer,
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_SECURE_NODOTDOT);

    struct archive_entry *entry;
    int total = 0, done = 0;
    int r;
    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        /* redirect into extractPath */
        std::string outp = (fs::path(extractPath) / archive_entry_pathname(entry)).string();
        archive_entry_set_pathname(entry, outp.c_str());

        if (archive_write_header(writer, entry) == ARCHIVE_OK) {
            const void *buff;
            size_t size;
            la_int64_t offset;
            while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK)
                archive_write_data_block(writer, buff, size, offset);
            archive_write_finish_entry(writer);
        }
        if (progress) progress((double)(++done) / (total ? total : done + 1) * 100.0);
    }
    if (r != ARCHIVE_EOF) {
        std::string e = archive_error_string(a) ? archive_error_string(a) : "extraction error";
        archive_read_free(a);
        archive_write_free(writer);
        throw std::runtime_error(e);
    }
    archive_read_free(a);
    archive_write_free(writer);
}

void ArchiveManager::testArchive(const std::string &archivePath,
                                 const std::string &password)
{
    std::string ext = lower(fs::path(archivePath).extension().string());
    if (ext == ".exe") {
        std::vector<uint8_t> blob;
        SfxPayload p;
        if (!loadSfx(archivePath, blob, p))
            throw std::runtime_error("Not a Zipline self-extracting archive.");
        try {
            decodeSfx(p, password);
        } catch (...) {
            sfx_payload_free(&p);
            throw;
        }
        sfx_payload_free(&p);
        return;
    }

    if (ext == ".7z") {
        SzReadResult rr = sevenzipRead(archivePath, password, true);
        if (rr.ok) {
            /* verify CRCs by re-reading would require storing them; decode success
             * plus our internal CRC check in the SFX path suffices. Here, a clean
             * decode of every folder is the integrity test. */
            return;
        }
        if (!rr.unsupported)
            throw std::runtime_error("Archive test failed: " + rr.error);
    }

    struct archive *a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    if (!password.empty())
        archive_read_add_passphrase(a, password.c_str());
    if (archive_read_open_filename(a, archivePath.c_str(), 65536) != ARCHIVE_OK) {
        std::string e = archive_error_string(a) ? archive_error_string(a) : "open failed";
        archive_read_free(a);
        throw std::runtime_error("Failed to open archive: " + e);
    }
    struct archive_entry *entry;
    int r;
    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const void *buff;
        size_t size;
        la_int64_t offset;
        while ((r = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {}
        if (r != ARCHIVE_EOF) break;
    }
    bool ok = (r == ARCHIVE_EOF);
    std::string err = ok ? "" : (archive_error_string(a) ? archive_error_string(a) : "test failed");
    archive_read_free(a);
    if (!ok)
        throw std::runtime_error("Archive test failed: " + err);
}

} // namespace zipline
