#pragma once

// Settings shared between the mod (inside Geometry Dash) and the separate
// tuner window. They talk through two small text files in one folder:
//
//   config.txt  - written by the tuner, read by the mod
//   status.txt  - written by the mod, read by the tuner (so it can show "connected")
//
// Folder: %APPDATA%\GDInputBuffer on Windows, ~/.gd-input-buffer elsewhere.
// This file does not use Geode, so it can be tested on its own.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace buffercfg {
    namespace fs = std::filesystem;
    using clock = std::chrono::steady_clock;

    struct Config {
        bool enabled = true;
        int windowMs = 50; // how long an early click stays "alive"
    };

    inline Config& current() {
        static Config c;
        return c;
    }

    // How many early clicks the mod has buffered since the game started.
    inline int& bufferedCount() {
        static int n = 0;
        return n;
    }

    inline fs::path dir() {
#ifdef _WIN32
        if (auto appdata = std::getenv("APPDATA")) return fs::path(appdata) / "GDInputBuffer";
#endif
        if (auto home = std::getenv("HOME")) return fs::path(home) / ".gd-input-buffer";
        return fs::path(".") / "gd-input-buffer";
    }

    inline std::string trim(std::string s) {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        return s;
    }

    // Re-read config.txt if the tuner changed it. Cheap enough to call every
    // physics step: it only touches the disk about 4 times per second.
    // Returns true if new values were loaded.
    inline bool reloadIfChanged(bool force = false) {
        static auto lastCheck = clock::now() - std::chrono::seconds(10);
        static fs::file_time_type lastWrite{};

        auto now = clock::now();
        if (!force && now - lastCheck < std::chrono::milliseconds(250)) return false;
        lastCheck = now;

        std::error_code ec;
        auto path = dir() / "config.txt";
        auto stamp = fs::last_write_time(path, ec);
        if (ec || stamp == lastWrite) return false;

        std::ifstream in(path);
        if (!in) return false; // the tuner may be mid-save; try again next time

        Config next = current(); // any missing line keeps its old value
        std::string line;
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            auto key = trim(line.substr(0, eq));
            auto val = trim(line.substr(eq + 1));
            try {
                if (key == "enabled") next.enabled = (val == "1");
                else if (key == "window_ms") next.windowMs = std::clamp(std::stoi(val), 0, 150);
            } catch (...) {
                // ignore a garbled value and keep the old one
            }
        }
        current() = next;
        lastWrite = stamp;
        return true;
    }

    // Tell the tuner we are alive. Written about once per second.
    inline void writeStatus(bool force = false) {
        static auto last = clock::now() - std::chrono::seconds(10);
        auto now = clock::now();
        if (!force && now - last < std::chrono::seconds(1)) return;
        last = now;

        std::error_code ec;
        fs::create_directories(dir(), ec);
        std::ofstream out(dir() / "status.txt", std::ios::trunc);
        if (!out) return;

        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        out << "heartbeat=" << secs << "\n";
        out << "buffered=" << bufferedCount() << "\n";
    }
}
