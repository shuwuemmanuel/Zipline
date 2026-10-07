#include "../core/archive_manager.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace zipline;

static int failures = 0;
static void check(bool c, const std::string &m)
{
    std::cout << (c ? "  PASS  " : "  FAIL  ") << m << "\n";
    if (!c) failures++;
}

static std::string readFile(const std::string &p)
{
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char **argv)
{
    std::string testdir = argc > 1 ? argv[1] : "test_files";
    std::string work = argc > 2 ? argv[2] : "/tmp/engine_work";
    fs::remove_all(work);
    fs::create_directories(work);

    ArchiveManager mgr;
    std::vector<std::string> files = {
        testdir + "/test1.txt", testdir + "/test2.txt",
        testdir + "/readme.md", testdir + "/data.json", testdir + "/config.ini"
    };

    auto roundtrip = [&](const std::string &fmt, const std::string &outname,
                         const ArchiveSettings &s, const std::string &pass) {
        std::string out = work + "/" + outname;
        try {
            mgr.createArchive(files, out, s, nullptr);
            check(fs::exists(out) || fs::exists(out + ".001"),
                  fmt + ": archive created");
            auto list = mgr.listContents(out, pass);
            check(list.size() == files.size(), fmt + ": listing has " +
                  std::to_string(list.size()) + " entries");
            std::string ex = work + "/ex_" + outname;
            mgr.extractArchive(out, ex, pass, nullptr);
            /* verify one file's content */
            std::string got;
            for (auto &p : fs::recursive_directory_iterator(ex))
                if (p.path().filename() == "data.json") got = readFile(p.path().string());
            check(got == readFile(testdir + "/data.json"),
                  fmt + ": extracted content matches");
        } catch (const std::exception &e) {
            check(false, fmt + ": exception: " + e.what());
        }
    };

    { ArchiveSettings s; s.format = "ZIP"; s.test_after_creation = true;
      roundtrip("ZIP", "a.zip", s, ""); }

    { ArchiveSettings s; s.format = "ZIP"; s.password_protected = true;
      s.password = "pw1"; s.encryption_method = "AES-256"; s.test_after_creation = false;
      roundtrip("ZIP+AES", "enc.zip", s, "pw1"); }

    { ArchiveSettings s; s.format = "7Z"; s.compression_level = 5;
      roundtrip("7Z", "a.7z", s, ""); }

    { ArchiveSettings s; s.format = "7Z"; s.password_protected = true; s.password = "pw2";
      s.test_after_creation = false;
      roundtrip("7Z+AES", "enc.7z", s, "pw2"); }

    { ArchiveSettings s; s.format = "TAR"; s.test_after_creation = true;
      roundtrip("TAR", "a.tar", s, ""); }

    { ArchiveSettings s; s.format = "GZIP"; s.compression_level = 6;
      roundtrip("GZIP", "a.tar.gz", s, ""); }

    { ArchiveSettings s; s.format = "EXE"; s.compression_level = 6;
      s.exe_advanced_settings["window_title"] = "Smoke Test";
      roundtrip("EXE", "a.exe", s, ""); }

    { ArchiveSettings s; s.format = "EXE"; s.password_protected = true; s.password = "pw3";
      roundtrip("EXE+AES", "enc.exe", s, "pw3"); }

    std::cout << "\n" << (failures ? "FAILURES: " : "ALL ENGINE TESTS PASSED (")
              << failures << (failures ? "" : ")") << "\n";
    return failures ? 1 : 0;
}
