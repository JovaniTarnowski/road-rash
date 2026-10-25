#include "importer.hpp"
#include "sha256.hpp"
#include "iso_fixture.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#ifndef _WIN32
#include <sys/stat.h>
#endif
int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("rr-import-test-" + std::to_string(SDL_GetTicksNS()));
    fs::create_directories(root / "source" / "ROADRASH" / "DATA");
    int failures = 0;
    auto check = [&](bool yes, const char* what) {
        if (!yes) {
            ++failures;
            std::cerr << "FAIL " << what << '\n';
        }
    };
    std::ofstream(root / "source" / "ROADRASH" / "A.BIN", std::ios::binary) << "hello";
    std::ofstream(root / "source" / "ROADRASH" / "DATA" / "B.BIN", std::ios::binary) << "abc";
    const std::vector<rr::ReferenceHash> hashes = {{"A.BIN", rr::sha256({'h', 'e', 'l', 'l', 'o'})},
                                                   {"DATA/B.BIN", rr::sha256({'a', 'b', 'c'})}};
    size_t done = 0, total = 0;
    const auto success = rr::import_game((root / "source").string(), (root / "success").string(), hashes,
                                         [&](const rr::ImportProgress& p) {
                                             done = p.done;
                                             total = p.total;
                                         });
    check(success.ok && done == 2 && total == 2, "verified directory import and progress");
    check(fs::file_size(root / "success" / "A.BIN") == 5 && fs::exists(root / "success" / ".rr-import"),
          "commit only complete verified files");
    std::ofstream(root / "source" / "ROADRASH" / "A.BIN", std::ios::binary) << "hellO";
    const auto corrupt = rr::import_game((root / "source").string(), (root / "failure").string(), hashes);
    check(!corrupt.ok && corrupt.mismatched == std::vector<std::string>{"A.BIN"} &&
              !fs::exists(root / "failure"),
          "corrupt import leaves no destination");
    fs::remove(root / "source" / "ROADRASH" / "DATA" / "B.BIN");
    const auto missing = rr::import_game((root / "source").string(), (root / "missing").string(), hashes);
    check(!missing.ok && missing.missing.size() == 1 && !fs::exists(root / "missing"),
          "missing file fails atomically");
    const auto retained = rr::import_game((root / "source").string(), (root / "success").string(), hashes);
    check(!retained.ok && !retained.error.empty() && fs::file_size(root / "success" / "A.BIN") == 5,
          "existing imported data preserved on retry");
    const auto unsafe = rr::import_game((root / "source").string(), (root / "unsafe").string(),
                                        {{"../escape", hashes[0].hash}});
    check(!unsafe.ok && !fs::exists(root / "escape"), "reference traversal rejected");
    const auto image = fixture::iso();
    {
        std::ofstream file(root / "disc.iso", std::ios::binary);
        file.write(reinterpret_cast<const char*>(image.data()), image.size());
    }
    const auto from_iso = rr::import_game((root / "disc.iso").string(), (root / "iso").string(), {hashes[0]});
    check(from_iso.ok && fs::file_size(root / "iso" / "A.BIN") == 5, "ISO import matches folder import");
    const auto canceled =
        rr::import_game((root / "disc.iso").string(), (root / "canceled").string(), {hashes[0]},
                        [](const rr::ImportProgress&) { throw std::runtime_error("canceled by test"); });
    check(!canceled.ok && canceled.error == "canceled by test" && !fs::exists(root / "canceled"),
          "cancellation discards partial import");
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    // OS pipe + writer thread. Emscripten's filesystem has no FIFOs and the
    // single-threaded web build has no pthreads; nonseekable ISO input on
    // the web goes through the pull-based ImportJob instead (plan 23 Task 4).
    const auto pipe = root / "disc-pipe";
    if (mkfifo(pipe.string().c_str(), 0600) != 0)
        check(false, "create nonseekable source");
    else {
        std::thread writer([&]() {
            std::ofstream file(pipe, std::ios::binary);
            file.write(reinterpret_cast<const char*>(image.data()), image.size());
        });
        const auto from_pipe = rr::import_game(pipe.string(), (root / "pipe").string(), {hashes[0]});
        writer.join();
        check(from_pipe.ok && fs::file_size(root / "pipe" / "A.BIN") == 5,
              "nonseekable provider cached and imported");
        check(!fs::exists(root / "pipe" / ".rr-source.iso"), "temporary ISO excluded from final data");
    }
#endif
    for (const auto& entry : fs::directory_iterator(root))
        check(entry.path().filename().string().find(".partial.") == std::string::npos, "staging cleanup");
    fs::remove_all(root);
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
