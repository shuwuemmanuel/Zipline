/*
 * Zipline Archive Manager - a professional archive creation and extraction tool
 * (C++ port of the original Python/PyQt6 application).
 *
 *   zipline                 # start the GUI application
 *   zipline --test          # run comprehensive tests
 *   zipline --demo-exe      # create a demo self-extracting EXE
 *   zipline --create-files  # create test files for the demo
 *   zipline --help          # show usage
 */

#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QTimer>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "core/archive_manager.h"
#include "ui/main_window.h"

namespace fs = std::filesystem;

static fs::path createTestFiles()
{
    std::cout << "Creating test files...\n";
    fs::path dir = "test_files";
    fs::create_directories(dir);
    const std::pair<const char *, const char *> files[] = {
        {"test1.txt", "This is test file 1\nHello World!"},
        {"test2.txt", "This is test file 2\nSecond file content"},
        {"readme.md", "# Test Files\n\nThese are test files for Zipline Archive Manager."},
        {"data.json", "{\"name\": \"test\", \"version\": \"1.0\", \"files\": [\"test1.txt\", \"test2.txt\"]}"},
        {"config.ini", "[settings]\ncompression=9\nformat=zip\npassword=false"}};
    for (auto &f : files) {
        std::ofstream out(dir / f.first, std::ios::binary);
        out << f.second;
    }
    std::cout << "Created 5 test files in '" << dir.string() << "' directory\n";
    return dir;
}

static bool runTests()
{
    std::cout << "Zipline Archive Manager - Test Suite\n";
    std::cout << "========================================\n";
    using namespace zipline;

    int passed = 0, total = 0;
    auto expect = [&](bool c, const std::string &m) {
        ++total;
        std::cout << (c ? "✓ " : "✗ ") << m << "\n";
        if (c) ++passed;
    };

    ArchiveManager mgr;
    auto fmts = mgr.supportedFormats();
    expect(!fmts.empty(), "Supported formats available");
    bool hasExe = false;
    for (auto &f : fmts) if (f == "EXE") hasExe = true;
    expect(hasExe, "EXE (Self-Extracting) format supported");

    if (!fs::exists("test_files") || fs::is_empty("test_files"))
        createTestFiles();

    std::vector<std::string> inputs;
    for (auto &p : fs::directory_iterator("test_files"))
        inputs.push_back(p.path().string());

    fs::path work = "test_out";
    fs::create_directories(work);

    try {
        ArchiveSettings s; s.format = "ZIP"; s.test_after_creation = true;
        mgr.createArchive(inputs, (work / "test.zip").string(), s);
        expect(fs::exists(work / "test.zip"), "ZIP creation + self-test");
        auto list = mgr.listContents((work / "test.zip").string());
        expect(list.size() == inputs.size(), "ZIP listing matches file count");
    } catch (const std::exception &e) { expect(false, std::string("ZIP: ") + e.what()); }

    try {
        ArchiveSettings s; s.format = "7Z"; s.compression_level = 5;
        mgr.createArchive(inputs, (work / "test.7z").string(), s);
        expect(fs::exists(work / "test.7z"), "7Z creation + self-test");
    } catch (const std::exception &e) { expect(false, std::string("7Z: ") + e.what()); }

    try {
        ArchiveSettings s; s.format = "7Z"; s.password_protected = true; s.password = "secret";
        s.test_after_creation = false;
        mgr.createArchive(inputs, (work / "enc.7z").string(), s);
        mgr.testArchive((work / "enc.7z").string(), "secret");
        expect(true, "7Z AES-256 encryption round-trip");
    } catch (const std::exception &e) { expect(false, std::string("7Z+AES: ") + e.what()); }

    try {
        ArchiveSettings s; s.format = "EXE"; s.compression_level = 6;
        s.exe_advanced_settings["window_title"] = "Test Package";
        mgr.createArchive(inputs, (work / "test.exe").string(), s);
        expect(fs::exists(work / "test.exe"), "EXE (self-extracting) creation");
        auto list = mgr.listContents((work / "test.exe").string());
        expect(list.size() == inputs.size(), "EXE payload listing matches file count");
    } catch (const std::exception &e) { expect(false, std::string("EXE: ") + e.what()); }

    std::cout << "\nResults: " << passed << "/" << total << " tests passed\n";
    if (passed == total) {
        std::cout << "✓ All tests passed! The application is ready to run.\n";
        return true;
    }
    std::cout << "✗ Some tests failed.\n";
    return false;
}

static bool demoExe()
{
    std::cout << "Zipline EXE Archive Demo\n==============================\n";
    using namespace zipline;
    if (!fs::exists("test_files") || fs::is_empty("test_files"))
        createTestFiles();
    std::vector<std::string> inputs;
    for (auto &p : fs::directory_iterator("test_files"))
        inputs.push_back(p.path().string());

    ArchiveManager mgr;
    ArchiveSettings s;
    s.format = "EXE";
    s.compression_level = 8;
    s.exe_advanced_settings = {
        {"window_title", "Zipline Demo Package - Advanced Features"},
        {"description", "This demo showcases advanced EXE features:\n"
                        "- Custom extraction paths\n- System integration\n- Professional installation"},
        {"use_zipline_icon", "1"},
        {"default_extract_path", "%USERPROFILE%\\Desktop\\ZiplineDemo"},
        {"create_subfolder", "0"},
        {"overwrite_files", "1"},
        {"open_folder", "1"},
        {"run_file", "1"},
        {"run_file_path", "readme.md"},
        {"silent_mode", "1"}};

    std::string out = "demo_archive.exe";
    std::cout << "Creating self-extracting executable: " << out << "\n";
    try {
        mgr.createArchive(inputs, out, s, [](double p) {
            static int last = -1; int step = (int)p / 20 * 20;
            if (step != last) { last = step; std::cout << "Progress: " << step << "%\n"; }
        });
    } catch (const std::exception &e) {
        std::cout << "✗ Error: " << e.what() << "\n";
        return false;
    }
    if (fs::exists(out)) {
        auto size = fs::file_size(out);
        std::cout << "\n✓ Self-extracting executable created successfully!\n";
        std::cout << "  File: " << out << "\n  Size: " << size << " bytes\n";
        std::cout << "\nRun it on Windows to extract (supports /S for silent mode).\n";
        return true;
    }
    std::cout << "✗ Failed to create executable\n";
    return false;
}

static int startGui(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("Zipline Archive Manager");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Zipline");
    app.setApplicationDisplayName("Zipline");

    for (const QString &p : {QString("Assets/zipline Logo.ico"), QString("Assets/zipline Logo.png")}) {
        if (QFile::exists(p)) { app.setWindowIcon(QIcon(p)); break; }
    }
    app.setStyle("Fusion");

    MainWindow window;
    window.show();

    /* Optional: render the window to a PNG and exit (used by CI / smoke tests). */
    if (const char *shot = std::getenv("ZIPLINE_SCREENSHOT")) {
        QString path = shot;
        QTimer::singleShot(1200, [&window, path]() {
            window.grab().save(path);
            QApplication::quit();
        });
    }
    return app.exec();
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--test") return runTests() ? 0 : 1;
        if (a == "--demo-exe") return demoExe() ? 0 : 1;
        if (a == "--create-files") { createTestFiles(); return 0; }
        if (a == "--help" || a == "-h") {
            std::cout << "Zipline Archive Manager\n\n"
                         "  zipline                 start the GUI application\n"
                         "  zipline --test          run comprehensive tests\n"
                         "  zipline --demo-exe      create a demo self-extracting EXE\n"
                         "  zipline --create-files  create test files for the demo\n";
            return 0;
        }
    }
    return startGui(argc, argv);
}
