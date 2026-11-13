// Slowdown recorder: keeps the last minute and a half of the game's diagnostic output (the
// Perf lines, frame rate, clocks, phone temperature...) in memory, and when the game runs
// slow for a few seconds, writes it all to a report in the game files folder:
//
//     <data folder>/logs/slowdowns/slowdown-<date>_<time>-map<map>-chunk<chunk>.txt
//
// so a slowdown that comes and goes can be looked at afterwards, without having had a log
// running at the time. Each report also has the start of the session (version, device, GPU
// driver, settings) and the 15 seconds after the slowdown began. The newest 20 are kept.
//
// Lines come from the stdout/stderr pipe (android_main.cpp), the perf monitor, the
// per-second frame rate (game_state.cpp) and the app (DK64Activity: temperature/thermal).

#include <android/log.h>
#include <jni.h>
#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
    constexpr const char* log_tag = "DK64Recomp";

    using Clock = std::chrono::steady_clock;

    constexpr auto keep_for = std::chrono::seconds(90);      // history kept in memory
    constexpr size_t max_lines = 4000;
    constexpr size_t startup_lines = 80;                       // the session's first lines, kept
    constexpr float slow_fps = 25.0f;                          // the game aims for 30
    constexpr int slow_seconds = 3;                            // this many slow seconds in a row
    constexpr auto after_trigger = std::chrono::seconds(15);   // keep recording this long, then write
    constexpr auto between_reports = std::chrono::minutes(2);
    constexpr auto same_place_gap = std::chrono::minutes(10);  // for the same map and chunk
    constexpr size_t max_reports = 20;

    struct Line {
        Clock::time_point when;
        std::string wall;   // wall-clock time, for matching with what the player saw
        std::string text;
    };

    std::mutex mutex;
    std::deque<Line> history;
    std::vector<std::string> startup;
    std::string partial;   // an unfinished line from the pipe
    std::string reports_dir;

    int slow_streak = 0;
    float slowest = 0.0f;
    bool pending = false;
    Clock::time_point pending_until, last_report;
    int last_map = -1, last_chunk = -1, pending_map = -1, pending_chunk = -1;
    std::string pending_started;

    std::string wall_time(bool for_file) {
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_r(&t, &tm);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), for_file ? "%Y-%m-%d_%H-%M-%S" : "%H:%M:%S", &tm);
        return buffer;
    }

    void add_locked(std::string text) {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
            text.pop_back();
        }
        if (text.empty()) {
            return;
        }
        const auto now = Clock::now();
        if (startup.size() < startup_lines) {
            startup.push_back(wall_time(false) + "  " + text);
        }
        history.push_back({ now, wall_time(false), std::move(text) });
        while (!history.empty() && (history.size() > max_lines || now - history.front().when > keep_for)) {
            history.pop_front();
        }
    }

    void write_report(std::vector<std::string> lines, std::vector<std::string> start, std::string dir,
        std::string name, std::string summary) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const std::string path = dir + "/" + name;
        FILE* f = std::fopen(path.c_str(), "w");
        if (f == nullptr) {
            __android_log_print(ANDROID_LOG_WARN, log_tag, "Slowdown recorder: couldn't write %s", path.c_str());
            return;
        }
        std::fprintf(f, "DK64 Recompiled slowdown report\n%s\n\n", summary.c_str());
        std::fprintf(f, "==== Start of the session ====\n");
        for (const auto& s : start) {
            std::fprintf(f, "%s\n", s.c_str());
        }
        std::fprintf(f, "\n==== The last %lld seconds up to and after the slowdown ====\n",
            (long long)std::chrono::duration_cast<std::chrono::seconds>(keep_for).count());
        for (const auto& s : lines) {
            std::fprintf(f, "%s\n", s.c_str());
        }
        std::fclose(f);
        __android_log_print(ANDROID_LOG_INFO, log_tag, "Slowdown recorded: %s", path.c_str());

        // Keep the newest reports only.
        std::vector<std::filesystem::path> reports;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
            if (e.path().filename().string().rfind("slowdown-", 0) == 0) {
                reports.push_back(e.path());
            }
        }
        std::sort(reports.begin(), reports.end());   // names start with the date and time
        for (size_t i = 0; i + max_reports < reports.size(); i++) {
            std::filesystem::remove(reports[i], ec);
        }
    }

    // Called with the mutex held: writes the report once the time after the slowdown is up.
    void maybe_write_locked() {
        if (!pending || Clock::now() < pending_until || reports_dir.empty()) {
            return;
        }
        pending = false;
        std::vector<std::string> lines;
        lines.reserve(history.size());
        for (const auto& l : history) {
            lines.push_back(l.wall + "  " + l.text);
        }
        char name[128];
        std::snprintf(name, sizeof(name), "slowdown-%s-map%02X-chunk%d.txt", pending_started.c_str(), pending_map, pending_chunk);
        char summary[256];
        std::snprintf(summary, sizeof(summary),
            "Slow from %s: map 0x%02X chunk %d, slowest second %.1f FPS (below %.0f FPS for %d+ seconds in a row).",
            pending_started.c_str(), pending_map, pending_chunk, slowest, slow_fps, slow_seconds);
        std::thread(write_report, std::move(lines), startup, reports_dir, std::string{name}, std::string{summary}).detach();
    }
}

// Every line of the game's own output (android_main.cpp's pipe), possibly several at once.
extern "C" void dk64_android_record_output(const char* data, size_t size) {
    std::lock_guard lock{mutex};
    partial.append(data, size);
    size_t start = 0, end;
    while ((end = partial.find('\n', start)) != std::string::npos) {
        add_locked(partial.substr(start, end - start));
        start = end + 1;
    }
    partial.erase(0, start);
    if (partial.size() > 4096) {
        add_locked(std::move(partial));
        partial.clear();
    }
}

// A single line from native code (perf monitor) or the app.
extern "C" void dk64_android_record_line(const char* line) {
    std::lock_guard lock{mutex};
    add_locked(line);
}

// Where reports go: <data folder>/logs/slowdowns.
extern "C" void dk64_android_set_slowdown_folder(const char* data_dir) {
    std::lock_guard lock{mutex};
    reports_dir = std::string{data_dir} + "/logs/slowdowns";
}

// The game's frame rate over the last second, and where the player is (game_state.cpp).
extern "C" void dk64_android_slowdown_second(float fps, int map, int chunk) {
    char line[96];
    std::snprintf(line, sizeof(line), "Frame rate: %.1f FPS (map 0x%02X chunk %d)", fps, map, chunk);
    std::lock_guard lock{mutex};
    add_locked(line);
    maybe_write_locked();

    if (fps >= slow_fps) {
        slow_streak = 0;
        return;
    }
    if (slow_streak++ == 0 || fps < slowest) {
        slowest = slow_streak == 1 ? fps : std::min(slowest, fps);
    }
    if (pending || slow_streak < slow_seconds) {
        return;
    }
    const auto now = Clock::now();
    if (last_report != Clock::time_point{} &&
        (now - last_report < between_reports || (map == last_map && chunk == last_chunk && now - last_report < same_place_gap))) {
        return;
    }
    pending = true;
    pending_until = now + after_trigger;
    pending_map = map;
    pending_chunk = chunk;
    pending_started = wall_time(true);
    last_report = now;
    last_map = map;
    last_chunk = chunk;
    __android_log_print(ANDROID_LOG_INFO, log_tag,
        "Slowdown: below %.0f FPS for %d seconds at map 0x%02X chunk %d; a report will be saved in %lld s",
        slow_fps, slow_seconds, map, chunk, (long long)std::chrono::duration_cast<std::chrono::seconds>(after_trigger).count());
}

// From DK64Activity: phone temperature and thermal status, every few seconds.
extern "C" JNIEXPORT void JNICALL
Java_io_github_bownz_dk64recomp_DK64Activity_nativeRecordLine(JNIEnv* env, jclass, jstring text) {
    if (text == nullptr) {
        return;
    }
    const char* chars = env->GetStringUTFChars(text, nullptr);
    if (chars != nullptr) {
        dk64_android_record_line(chars);
        env->ReleaseStringUTFChars(text, chars);
    }
}
