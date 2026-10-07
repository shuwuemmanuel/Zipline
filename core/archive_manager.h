/*
 * archive_manager.h - C++ port of the original Python ArchiveManager.
 *
 * Handles creation, extraction, listing and testing of ZIP, 7Z, EXE (self-
 * extracting), TAR and GZIP archives, plus extraction of RAR/BZ2/XZ.  The
 * public surface mirrors the Python class so the Qt UI maps onto it 1:1.
 */
#ifndef ZIPLINE_ARCHIVE_MANAGER_H
#define ZIPLINE_ARCHIVE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

namespace zipline {

/* One row in the file list / archive listing. */
struct ArchiveEntry {
    std::string name;
    uint64_t    size = 0;
    uint64_t    compressed_size = 0;
    std::string type;
    std::string modified;
    std::string path;
};

/* Settings dictionary, mirroring the Python settings keys. */
struct ArchiveSettings {
    std::string format = "ZIP";
    int         compression_level = 6;
    std::string compression_method = "Deflate";
    std::string dictionary_size = "4 MB";
    bool        solid_archive = true;
    bool        split_archive = false;
    int         volume_size = 100;        /* MB */
    bool        delete_after = false;
    bool        store_attributes = true;
    bool        store_symlinks = true;
    bool        password_protected = false;
    std::string password;
    std::string encryption_method = "AES-256";
    bool        encrypt_filenames = false;
    std::string comment;
    bool        test_after_creation = true;
    bool        show_progress = true;
    /* advanced EXE/SFX settings (key -> value, mirrors Python dict) */
    std::map<std::string, std::string> exe_advanced_settings;
};

using ProgressCallback = std::function<void(double)>; /* 0-100 */

class ArchiveManager {
public:
    ArchiveManager();

    /* Supported create formats and extract extensions (for the UI / tests). */
    std::vector<std::string> supportedFormats() const;
    std::vector<std::string> extractFormats() const;

    /* Create an archive from a list of files/folders. Throws std::runtime_error. */
    void createArchive(const std::vector<std::string> &files,
                       const std::string &outputPath,
                       const ArchiveSettings &settings,
                       ProgressCallback progress = nullptr);

    /* Extract an archive to a directory. */
    void extractArchive(const std::string &archivePath,
                        const std::string &extractPath,
                        const std::string &password = "",
                        ProgressCallback progress = nullptr);

    /* List the contents of an archive. */
    std::vector<ArchiveEntry> listContents(const std::string &archivePath,
                                           const std::string &password = "");

    /* Test archive integrity (throws on failure). */
    void testArchive(const std::string &archivePath,
                     const std::string &password = "");

private:
    /* Expand folders into their files, recording archive-relative names. */
    void expandInputs(const std::vector<std::string> &files,
                      std::vector<std::string> &diskPaths,
                      std::vector<std::string> &arcNames);

    void createZip(const std::vector<std::string> &disk,
                   const std::vector<std::string> &arc,
                   const std::string &out, const ArchiveSettings &s,
                   ProgressCallback progress);
    void createTarLike(const std::vector<std::string> &disk,
                       const std::vector<std::string> &arc,
                       const std::string &out, const ArchiveSettings &s,
                       bool gzip, ProgressCallback progress);
    void create7z(const std::vector<std::string> &disk,
                  const std::vector<std::string> &arc,
                  const std::string &out, const ArchiveSettings &s,
                  ProgressCallback progress);
    void createExe(const std::vector<std::string> &disk,
                   const std::vector<std::string> &arc,
                   const std::string &out, const ArchiveSettings &s,
                   ProgressCallback progress);

    void splitIntoVolumes(const std::string &path, uint64_t volumeBytes);

    std::string stubPath_;   /* path to the SFX stub executable */
};

/* Location of the bundled SFX stub; overridable via ZIPLINE_SFX_STUB. */
std::string defaultStubPath();

} // namespace zipline

#endif
