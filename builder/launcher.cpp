// SparkPark launcher: the one-time builder (CLAUDE.md, "Launcher (one-time builder)").
//
// What the player runs. If a completed build of this version exists it starts the game and exits;
// otherwise it shows the builder page (ui/index.html, made from the design reference by
// make_ui.py) in a WebView2 window and runs the real steps behind its button:
//   pick  -> file picker -> the disc check (disc_verify.c, upstream's, one path for every
//            platform): "dtk disc info" refuses another game, region or revision; "dtk disc
//            verify" against Redump: an image that differs (WBFS, scrubbed) gets a warning the
//            player can build past, or go back from (Esc / B);
//   build -> extract (dtk) -> main.dol checked (a patched one is refused)
//            -> recompile (embedded Python, tools/recomp/recomp.py)
//            -> compile + link (llvm-mingw, tools/sparkpark/build_player.py)
//   play  -> start the game, close the builder.
//
// Updating: the builder is one-time per version. When build.json was written by another
// SparkPark version, the builder opens ready to rebuild: from the disc files already extracted
// (checked complete, main.dol checked), so no disc image is needed; if they aren't there, it asks
// for the disc again. Before any build over an existing save, the save (game\R8AE01\nand) and
// settings.ini are copied to backups\<date_time>; the newest 10 are kept. "Restore a backup" on
// the page puts one back (the current save is backed up first). Saves are never deleted.
//
// Everything built from the player's disc stays on the player's PC, in SPARKPARK_HOME:
//   %LOCALAPPDATA%\SparkPark            by default;
//   <launcher folder>\data              when portable.txt sits beside the launcher;
//   %SPARKPARK_HOME%                    when set.
// Inside it: game\SparkParkGame.exe, game\R8AE01\{disc,sys,nand}, game\settings.ini,
// build.json (the "build complete" marker), backups\, logs\build.log, work\ (deleted after a build).
//
// Test hooks (environment, for unattended runs): SPARKPARK_TEST_ISO (skip the file picker),
// SPARKPARK_AUTOPRESS=<ms> (press the button when each state settles), SPARKPARK_SHOTS=<dir>
// (save a PNG of each state), SPARKPARK_NO_LAUNCH=1 (Play closes instead of starting the game),
// SPARKPARK_TEST_FAIL=compile (point the build at a missing compiler, to see a failed build).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wrl.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <algorithm>

#include "WebView2.h"
#include "disc_verify.h"
#include "sparkpark_version.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

static const char kVersion[] = SPARKPARK_VERSION;
static const int kKeepBackups = 10;
// Bytes in the files of R8AE01 Rev 0's data partition (all 373 extracted files), for the
// extraction's progress. The disc is verified before extracting, so this is exact.
static const unsigned long long kExtractBytes = 1400709637ULL;
static const int kGenFiles = 73;  // recomp_*.c files recomp.py writes for R8AE01 Rev 0

enum { WM_APP_POST = WM_APP + 1, WM_APP_DONE_PICK };
static HWND gWnd;
static ComPtr<ICoreWebView2Controller> gCtl;
static ComPtr<ICoreWebView2> gView;
static fs::path gApp, gHome;
static std::wstring gIso;
static std::string gIsoSha1;        // the image's SHA-1 as "dtk disc verify" printed it
static bool gIsoOriginal = false;   // it matched Redump's (else the player built past the warning)
static std::string gBuiltVersion;   // build.json's version, if a build exists
static std::string gPrevDiscSha1, gPrevImage, gPrevCheck;  // and what it says about the disc
static bool gFromExtracted = false; // rebuild from the disc files already extracted (an update)
static std::atomic<bool> gBusy{false};
static std::mutex gLogMutex;

// ------------------------------------------------------------------------------------------
static std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
static std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
static std::string json_str(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c == '\n') o += "\\n";
        else if (c < 0x20) o += ' ';
        else o += (char)c;
    }
    return o + "\"";
}
static std::wstring env(const wchar_t* name) {
    wchar_t buf[2048];
    DWORD n = GetEnvironmentVariableW(name, buf, 2048);
    return n && n < 2048 ? std::wstring(buf, n) : std::wstring();
}

// Sends a message to the page (from any thread).
static void post(const std::string& json) {
    PostMessageW(gWnd, WM_APP_POST, 0, (LPARAM) new std::wstring(widen(json)));
}

static fs::path log_path() { return gHome / "logs" / "build.log"; }
static void log_line(const std::string& s) {
    std::lock_guard<std::mutex> g(gLogMutex);
    std::ofstream(log_path(), std::ios::app | std::ios::binary) << s << "\n";
}

// The process run() is waiting on, for a tick that wants its I/O counters.
static thread_local HANDLE tChild;

// Every child (dtk, Python, clang) goes in this job, which ends them when the launcher closes:
// closing the window mid-build must not leave a build running in the background.
static HANDLE children_job() {
    static HANDLE job = [] {
        HANDLE j = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (j) SetInformationJobObject(j, JobObjectExtendedLimitInformation, &li, sizeof(li));
        return j;
    }();
    return job;
}

// Runs a command; each line of its output goes to the log and to on_line; tick runs about
// every 200 ms while it runs. Returns the exit code, or -1 if it couldn't start.
static int run(const std::wstring& cmd, const fs::path& cwd, const std::map<std::wstring, std::wstring>& extra_env,
               const std::function<void(const std::string&)>& on_line, const std::function<void()>& tick,
               std::string* all = nullptr) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE rd, wr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    // The environment: the launcher's own, with extra_env set over it.
    std::map<std::wstring, std::wstring> vars;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (wchar_t* p = block; *p; p += wcslen(p) + 1) {
            std::wstring kv = p;
            size_t eq = kv.find(L'=', 1);
            if (eq != std::wstring::npos) vars[kv.substr(0, eq)] = kv.substr(eq + 1);
        }
        FreeEnvironmentStringsW(block);
    }
    for (auto& [k, v] : extra_env) vars[k] = v;
    std::wstring envblock;
    for (auto& [k, v] : vars) envblock += k + L"=" + v + L'\0';
    envblock += L'\0';
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = si.hStdError = wr;
    PROCESS_INFORMATION pi{};
    std::wstring line = cmd;
    log_line("$ " + narrow(cmd));
    BOOL ok = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, envblock.data(),
                             cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) {
        CloseHandle(rd);
        log_line("  (could not start)");
        return -1;
    }
    // In the job before it runs, so its own children (clang under Python) are in it too.
    if (HANDLE job = children_job()) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    std::string pending;
    auto drain = [&]() {
        DWORD avail = 0, got = 0;
        while (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail) {
            char buf[4096];
            if (!ReadFile(rd, buf, std::min<DWORD>(avail, sizeof(buf)), &got, nullptr) || !got) break;
            pending.append(buf, got);
            if (all) all->append(buf, got);
            size_t nl;
            while ((nl = pending.find('\n')) != std::string::npos) {
                std::string l = pending.substr(0, nl);
                if (!l.empty() && l.back() == '\r') l.pop_back();
                pending.erase(0, nl + 1);
                log_line("  " + l);
                if (on_line) on_line(l);
            }
        }
    };
    tChild = pi.hProcess;
    while (WaitForSingleObject(pi.hProcess, 200) == WAIT_TIMEOUT) {
        drain();
        if (tick) tick();
    }
    tChild = nullptr;
    drain();
    if (!pending.empty()) {
        log_line("  " + pending);
        if (on_line) on_line(pending);
    }
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(rd);
    log_line("  (exit " + std::to_string(code) + ")");
    return (int)code;
}

static std::wstring q(const fs::path& p) { return L"\"" + p.wstring() + L"\""; }

static unsigned long long tree_bytes(const fs::path& dir) {
    unsigned long long n = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec))
        if (it->is_regular_file(ec)) n += it->file_size(ec);
    return n;
}

// ------------------------------------------------------------------------------------------
// The disc check (pick).
// The short reason shown on the sign (it has room for about two lines; disc_verify.c's full
// explanation goes to the log).
static std::string short_reason(disc_result r, const char* msg) {
    switch (r) {
    case DISC_WRONG_GAME: return "That's a different game. SparkPark needs PokePark Wii (USA).";
    case DISC_WRONG_REGION: {
        std::string m = msg, which = "another region's release";
        size_t a = m.find("That's "), b = m.find(" of PokePark");
        if (a == 0 && b != std::string::npos) which = m.substr(7, b - 7);
        return "Wrong region: that's " + which + ". SparkPark needs the USA disc (R8AE01).";
    }
    case DISC_WRONG_REVISION: return "Wrong revision: SparkPark needs the first release (Rev 0) of the USA disc.";
    case DISC_WRONG_CODE: return "That copy's game code is patched or modded. Use an unmodified copy of your disc.";
    default: return "That file couldn't be read as a disc image. Pick an ISO, RVZ or WBFS.";
    }
}

// The text after "SHA-1 :" in dtk's verify output: the image's own hash, whatever it is.
static std::string sha1_of(const std::string& verify) {
    size_t p = verify.find("SHA-1 :");
    if (p == std::string::npos) return {};
    p += 7;
    while (p < verify.size() && verify[p] == ' ') p++;
    std::string h;
    while (p < verify.size() && isxdigit((unsigned char)verify[p]) && h.size() < 40) h += (char)tolower(verify[p++]);
    return h.size() == 40 ? h : std::string();
}

static void verify_thread(std::wstring iso) {
    post(R"({"type":"verifying"})");
    long long size = 0;
    if (FILE* f = _wfopen(iso.c_str(), L"rb")) {
        _fseeki64(f, 0, SEEK_END);
        size = _ftelli64(f);
        fclose(f);
    }
    fs::path dtk = gApp / "tools" / "dtk.exe";
    std::string info, verify;
    char msg[512] = "";
    log_line("== disc check: " + narrow(iso));
    int rc = run(q(dtk) + L" disc info " + q(iso), {}, {}, nullptr, nullptr, &info);
    disc_result r = rc == -1 ? DISC_UNREADABLE : disc_check_info(info.c_str(), msg, sizeof msg);
    if (r != DISC_OK) {
        std::string why = rc == -1 ? "Couldn't start tools\\dtk.exe, which reads disc images. Is the SparkPark folder complete?"
                                   : short_reason(r, msg);
        log_line(std::string("disc refused: ") + (rc == -1 ? why : msg));
        post(std::string(R"({"type":"discError","reason":)") + json_str(why) + "}");
        gBusy = false;
        return;
    }
    // Hashing reads the whole image once: from a USB hard drive that's minutes, so show how far
    // dtk has read (its I/O counters against the file's size).
    int last = -1;
    run(q(dtk) + L" disc verify " + q(iso), {}, {}, nullptr,
        [&] {
            IO_COUNTERS io{};
            if (!tChild || size <= 0 || !GetProcessIoCounters(tChild, &io)) return;
            int pct = (int)std::min<double>(99, 100.0 * io.ReadTransferCount / size);
            if (pct != last) {
                last = pct;
                post(R"({"type":"verifyProgress","pct":)" + std::to_string(pct) + "}");
            }
        },
        &verify);
    gIso = iso;
    gIsoSha1 = sha1_of(verify);
    gIsoOriginal = disc_is_original(verify.c_str()) != 0;
    gFromExtracted = false;
    if (gIsoOriginal) {
        log_line("disc OK: the original disc (Redump)");
        post(R"({"type":"verified"})");
    } else {
        // The right game, region and revision, but not byte for byte the original disc: WBFS and
        // scrubbed copies leave out the disc's padding, and usually play fine. main.dol is
        // checked after extracting; a patched one is refused then.
        log_line("disc differs from the original (WBFS, scrubbed or the like); the player may build anyway");
        post(R"({"type":"verified","warn":"Not an exact copy of the disc (WBFS or scrubbed?). It usually plays fine: build, or press Esc to pick another."})");
    }
    gBusy = false;
}

// ------------------------------------------------------------------------------------------
// Save backups: game\R8AE01\nand (the saves and photos) and game\settings.ini, copied to
// backups\<date_time>. The newest kKeepBackups are kept; the live save is never touched here.
static fs::path backups_dir() { return gHome / "backups"; }

static std::vector<fs::path> list_backups() {
    std::vector<fs::path> v;
    std::error_code ec;
    for (auto it = fs::directory_iterator(backups_dir(), ec); !ec && it != fs::directory_iterator(); it.increment(ec))
        if (it->is_directory(ec) && fs::exists(it->path() / "nand", ec)) v.push_back(it->path());
    std::sort(v.begin(), v.end());  // the names are timestamps: oldest first
    return v;
}

// Returns the backup's folder, or an empty path if there was no save to back up (or it failed).
static fs::path backup_save(const std::string& reason) {
    std::error_code ec;
    const fs::path game = gHome / "game", nand = game / "R8AE01" / "nand";
    if (!fs::exists(nand, ec) || fs::is_empty(nand, ec)) return {};
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf(name, 64, L"%04u-%02u-%02u_%02u-%02u-%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    fs::path dst = backups_dir() / name;
    for (int i = 2; fs::exists(dst, ec); i++) dst = backups_dir() / (std::wstring(name) + L"_" + std::to_wstring(i));
    fs::create_directories(dst, ec);
    fs::copy(nand, dst / "nand", fs::copy_options::recursive, ec);
    if (ec) {
        log_line("backup failed: " + ec.message());
        fs::remove_all(dst, ec);
        return {};
    }
    if (fs::exists(game / "settings.ini")) fs::copy_file(game / "settings.ini", dst / "settings.ini", ec);
    std::ofstream(dst / "backup.txt", std::ios::binary)
        << "SparkPark save backup\nreason: " << reason << "\nmade by: SparkPark " << kVersion
        << "\nbuilt game version: " << (gBuiltVersion.empty() ? "unknown" : gBuiltVersion) << "\n";
    log_line("save backed up (" + reason + "): " + narrow(dst.wstring()));
    std::vector<fs::path> all = list_backups();
    for (size_t i = 0; all.size() > (size_t)kKeepBackups && i < all.size() - kKeepBackups; i++) {
        fs::remove_all(all[i], ec);
        log_line("old backup removed: " + narrow(all[i].wstring()));
    }
    return dst;
}

// "Restore a backup": the player picks one of backups\*, the current save is backed up first,
// then the backup's nand and settings.ini replace the current ones.
static void restore_backup() {
    std::error_code ec;
    ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(L"Choose a save backup to restore (a folder named by its date and time)");
    ComPtr<IShellItem> folder;
    fs::create_directories(backups_dir(), ec);
    if (SUCCEEDED(SHCreateItemFromParsingName(backups_dir().c_str(), nullptr, IID_PPV_ARGS(&folder))))
        dlg->SetFolder(folder.Get());
    if (FAILED(dlg->Show(gWnd))) return;
    ComPtr<IShellItem> item;
    PWSTR picked = nullptr;
    if (FAILED(dlg->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &picked))) return;
    fs::path src = picked;
    CoTaskMemFree(picked);
    if (!fs::exists(src / "nand")) {
        MessageBoxW(gWnd, L"That folder isn't a SparkPark save backup (it has no nand folder).", L"SparkPark",
                    MB_ICONWARNING);
        return;
    }
    std::wstring ask = L"Replace your current save and settings with the backup from " + src.filename().wstring() +
                       L"?\n\nYour current save is backed up first, so this can be undone.";
    if (MessageBoxW(gWnd, ask.c_str(), L"SparkPark", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    const fs::path game = gHome / "game", nand = game / "R8AE01" / "nand";
    backup_save("before restoring " + narrow(src.filename().wstring()));
    fs::path tmp = game / "R8AE01" / "nand.restoring";
    fs::remove_all(tmp, ec);
    fs::copy(src / "nand", tmp, fs::copy_options::recursive, ec);
    if (ec) {
        fs::remove_all(tmp, ec);
        post(R"({"type":"note","error":true,"text":"Couldn't restore that backup. Your save is unchanged."})");
        return;
    }
    fs::remove_all(nand, ec);
    fs::rename(tmp, nand, ec);
    if (ec) {  // (the folder in use?) copy it over instead
        fs::copy(tmp, nand, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        fs::remove_all(tmp, ec);
    }
    if (fs::exists(src / "settings.ini")) fs::copy_file(src / "settings.ini", game / "settings.ini", fs::copy_options::overwrite_existing, ec);
    log_line("restored the save backup " + narrow(src.wstring()));
    post(std::string(R"({"type":"note","text":)") + json_str("Save restored from " + narrow(src.filename().wstring())) +
         R"(,"backups":)" + std::to_string(list_backups().size()) + "}");
}

// ------------------------------------------------------------------------------------------
// The build.
static void progress(double pct, const char* stage) {
    char buf[96];
    snprintf(buf, sizeof buf, R"({"type":"progress","pct":%.1f,"stage":"%s"})", pct, stage);
    post(buf);
}

// The disc files already extracted (an earlier build's): all there, byte count and main.dol.
static bool extracted_complete() {
    const fs::path disc = gHome / "game" / "R8AE01" / "disc";
    return fs::exists(disc / "sys" / "main.dol") && tree_bytes(disc) == kExtractBytes;
}

// main.dol against the one the code is recompiled from (disc_verify.c). 0 if it's the original.
static bool dol_is_original(const fs::path& dol, std::string& why) {
    std::ifstream f(dol, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    char msg[512] = "";
    if (!bytes.empty() && disc_check_dol(bytes.data(), bytes.size(), msg, sizeof msg) == DISC_OK) return true;
    log_line(std::string("main.dol refused: ") + (bytes.empty() ? "unreadable" : msg));
    why = short_reason(DISC_WRONG_CODE, msg);
    return false;
}

// false: failed (why says why); refuse_disc set when the disc itself is refused (a patched
// main.dol), so the page goes back to Upload ISO instead of offering a retry.
static bool build_steps(std::string& why, bool& refuse_disc) {
    std::error_code ec;
    const fs::path game = gHome / "game", r8 = game / "R8AE01", disc = r8 / "disc", sys = r8 / "sys";
    const fs::path work = gHome / "work", gen = work / "gen";
    const fs::path recompiler = gApp / "recompiler", python = gApp / "toolchain" / "python" / "python.exe";
    fs::path llvm = gApp / "toolchain" / "llvm-mingw";
    if (env(L"SPARKPARK_TEST_FAIL") == L"compile") llvm = gApp / "toolchain" / "missing-compiler";
    refuse_disc = false;
    fs::remove(gHome / "build.json", ec);
    fs::remove_all(work, ec);
    fs::create_directories(gen, ec);
    int rc;

    // 1. Extract (0-12%), or, for an update, use the files an earlier build extracted. The saves
    //    in R8AE01\nand are kept either way.
    progress(0, "extract");
    fs::create_directories(sys, ec);
    if (gFromExtracted && extracted_complete()) {
        log_line("using the disc files already extracted (" + std::to_string(kExtractBytes) + " bytes)");
    } else {
        if (gIso.empty()) {
            why = "The disc files from the last build are missing. Choose your disc image again.";
            refuse_disc = true;
            return false;
        }
        fs::remove_all(disc, ec);
        rc = run(q(gApp / "tools" / "dtk.exe") + L" disc extract -q " + q(gIso) + L" " + q(disc), {}, {}, nullptr,
                 [&] { progress(12.0 * std::min(1.0, (double)tree_bytes(disc) / kExtractBytes), "extract"); });
        if (rc != 0 || !fs::exists(disc / "sys" / "main.dol")) {
            why = "Extracting the disc failed. Is there enough free space? (about 2 GB)";
            return false;
        }
    }
    // The recompiled code is made from one main.dol: a patched one can't run (upstream's check).
    if (!dol_is_original(disc / "sys" / "main.dol", why)) {
        fs::remove_all(disc, ec);
        refuse_disc = true;
        return false;
    }
    if (!fs::copy_file(disc / "sys" / "main.dol", sys / "main.dol", fs::copy_options::overwrite_existing, ec)) {
        why = "Couldn't copy main.dol: " + ec.message();
        return false;
    }

    // 2. Recompile main.dol to C (12-25%).
    progress(12, "recompile");
    rc = run(q(python) + L" " + q(recompiler / "tools" / "recomp" / "recomp.py") + L" --dol " + q(sys / "main.dol") +
                 L" --out " + q(gen) + L" --symbols " + q(recompiler / "config" / "R8AE01" / "symbols.txt") +
                 L" --hooks " + q(recompiler / "recomp" / "patches" / "hooks.txt"),
             recompiler, {}, nullptr, [&] {
                 int n = 0;
                 for (auto it = fs::directory_iterator(gen, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
                     if (it->path().filename().wstring().rfind(L"recomp_", 0) == 0) n++;
                 progress(12 + 13.0 * std::min(1.0, (double)n / kGenFiles), "recompile");
             });
    if (rc != 0) {
        why = rc == -1 ? "Couldn't start the bundled Python. Is the SparkPark folder complete?"
                       : "Recompiling the game code failed.";
        return false;
    }

    // 3. Compile (25-97%) and link (97-100%).
    progress(25, "recompile");
    std::string first_error;
    bool no_compiler = false;
    rc = run(q(python) + L" " + q(recompiler / "tools" / "sparkpark" / "build_player.py") + L" game --gen " + q(gen) +
                 L" --runtime " + q(gApp / "runtime") + L" --work " + q(work) + L" --exe " + q(game / "SparkParkGame.exe"),
             recompiler, {{L"SPARKPARK_LLVM", llvm.wstring()}},
             [&](const std::string& l) {
                 int done, total;
                 if (sscanf(l.c_str(), "[%d/%d]", &done, &total) == 2 && total > 0)
                     progress(25 + 72.0 * done / total, "recompile");
                 else if (l == "linking")
                     progress(97, "link");
                 else {
                     if (l.find("FileNotFoundError") != std::string::npos || l.find("WinError 2") != std::string::npos)
                         no_compiler = true;
                     std::string low = l;
                     for (char& c : low) c = (char)tolower((unsigned char)c);
                     // clang's own messages ("x.c:12:3: error: ...") say what went wrong.
                     if (first_error.empty() && low.find(": error:") != std::string::npos)
                         first_error = l.size() > 90 ? l.substr(0, 90) + "..." : l;
                 }
             },
             nullptr);
    if (rc != 0 || !fs::exists(game / "SparkParkGame.exe")) {
        why = no_compiler ? "The bundled compiler couldn't be started. Is the SparkPark folder complete?"
              : !first_error.empty() ? "Compiling failed: " + first_error
                                     : "Building the game failed.";
        return false;
    }
    fs::copy(gApp / "prompts", game / "prompts", fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    // The "build complete" marker; later starts of this version go straight into the game. For a
    // rebuild from the extracted files, the disc's hash and image path are the last build's.
    std::string disc_sha1 = gIsoSha1, image = narrow(gIso), check = gIsoOriginal ? "original" : "differs, built anyway";
    if (gFromExtracted && gIso.empty()) {
        disc_sha1 = gPrevDiscSha1.empty() ? DISC_REDUMP_SHA1 : gPrevDiscSha1;
        image = gPrevImage;
        check = gPrevCheck.empty() ? "original" : gPrevCheck;
    }
    std::ofstream(gHome / "build.json", std::ios::binary)
        << "{\n  \"version\": \"" << kVersion << "\",\n  \"disc\": \"R8AE01 Rev 0\",\n  \"disc_sha1\": "
        << json_str(disc_sha1) << ",\n  \"disc_check\": " << json_str(check) << ",\n  \"image\": " << json_str(image)
        << ",\n  \"main_dol_sha1\": \"" << DISC_MAIN_DOL_SHA1 << "\",\n  \"game\": \"game\\\\SparkParkGame.exe\"\n}\n";
    fs::remove_all(work, ec);
    progress(100, "link");
    return true;
}

static void build_thread() {
    log_line("== build, SparkPark " + std::string(kVersion) +
             (gBuiltVersion.empty() ? "" : " (the game was built by " + gBuiltVersion + ")"));
    // A save from before is backed up first: a rebuild keeps it, but an update is the moment
    // something could go wrong with it.
    if (fs::path b = backup_save(gBuiltVersion.empty() || gBuiltVersion == kVersion
                                     ? std::string("before rebuilding")
                                     : "before rebuilding for SparkPark " + std::string(kVersion));
        !b.empty())
        post(std::string(R"({"type":"backups","count":)") + std::to_string(list_backups().size()) + "}");
    std::string why;
    bool refuse_disc = false;
    auto t0 = std::chrono::steady_clock::now();
    bool ok = build_steps(why, refuse_disc);
    log_line(ok ? "build complete in " +
                      std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count()) + " s"
                : "build failed: " + why);
    if (ok) post(R"({"type":"done"})");
    else if (refuse_disc) {
        gIso.clear();
        gFromExtracted = false;
        post(std::string(R"({"type":"discError","reason":)") + json_str(why) + "}");
    } else post(std::string(R"({"type":"buildError","reason":)") + json_str(why + " See logs\\build.log.") + "}");
    gBusy = false;
}

// ------------------------------------------------------------------------------------------
// A string value from build.json (written above; v0.2.0's has version, disc, disc_sha1, game).
static std::string build_json_value(const std::string& json, const std::string& key) {
    size_t p = json.find("\"" + key + "\"");
    if (p == std::string::npos || (p = json.find(':', p)) == std::string::npos || (p = json.find('"', p)) == std::string::npos)
        return {};
    std::string v;
    for (p++; p < json.size() && json[p] != '"'; p++) {
        if (json[p] == '\\' && p + 1 < json.size()) p++;
        v += json[p];
    }
    return v;
}

static void read_build_json() {
    std::ifstream f(gHome / "build.json", std::ios::binary);
    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    gBuiltVersion = build_json_value(json, "version");
    gPrevDiscSha1 = build_json_value(json, "disc_sha1");
    gPrevImage = build_json_value(json, "image");
    gPrevCheck = build_json_value(json, "disc_check");
}

static bool build_complete() {
    return fs::exists(gHome / "build.json") && fs::exists(gHome / "game" / "SparkParkGame.exe") &&
           fs::exists(gHome / "game" / "R8AE01" / "sys" / "main.dol");
}

static bool launch_game() {
    fs::path exe = gHome / "game" / "SparkParkGame.exe";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    std::wstring cmd = q(exe);
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, exe.parent_path().c_str(), &si, &pi))
        return false;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

static void pick() {
    if (gBusy.exchange(true)) return;
    std::wstring iso = env(L"SPARKPARK_TEST_ISO");
    if (iso.empty()) {
        wchar_t file[MAX_PATH] = L"";
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = gWnd;
        ofn.lpstrFilter = L"Disc images (*.iso;*.rvz;*.wia;*.wbfs;*.ciso;*.gcz)\0*.iso;*.rvz;*.wia;*.wbfs;*.ciso;*.gcz\0All files\0*.*\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Choose your PokePark Wii (USA) disc image";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameW(&ofn)) {
            gBusy = false;
            return;
        }
        iso = file;
    }
    std::thread(verify_thread, iso).detach();
}

// Saves a PNG of the page when SPARKPARK_SHOTS is set (test runs), once per state.
static void screenshot(const std::wstring& name) {
    static std::wstring dir = env(L"SPARKPARK_SHOTS");
    static std::map<std::wstring, bool> taken;
    if (dir.empty() || taken[name]) return;
    taken[name] = true;
    std::thread([name] {
        // Let the state's animations play in first (the sign rises in 1.2 s; captions pop in).
        Sleep(name == L"upload" ? 2600 : 900);
        PostMessageW(gWnd, WM_APP_POST, 1, (LPARAM) new std::wstring(name));
    }).detach();
}
static void capture(const std::wstring& name) {
    fs::create_directories(env(L"SPARKPARK_SHOTS"));
    fs::path out = fs::path(env(L"SPARKPARK_SHOTS")) / (name + L".png");
    ComPtr<IStream> stream;
    if (FAILED(SHCreateStreamOnFileEx(out.c_str(), STGM_CREATE | STGM_WRITE, FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &stream)))
        return;
    gView->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, stream.Get(),
                          Callback<ICoreWebView2CapturePreviewCompletedHandler>([stream](HRESULT) -> HRESULT {
                              return S_OK;
                          }).Get());
}

static bool gUpdate = false;  // build.json is from another SparkPark version

static void on_message(const std::wstring& json) {
    auto has = [&](const wchar_t* s) { return json.find(s) != std::wstring::npos; };
    if (has(L"\"cmd\":\"ready\"")) {
        // What the page opens on: a fresh build, or an update's rebuild, and the backups there are.
        post(std::string(R"({"type":"init","update":)") + (gUpdate ? "true" : "false") +
             R"(,"fromExtracted":)" + (gFromExtracted ? "true" : "false") + R"(,"from":)" + json_str(gBuiltVersion) +
             R"(,"to":)" + json_str(kVersion) + R"(,"backups":)" + std::to_string(list_backups().size()) + "}");
    } else if (has(L"\"cmd\":\"back\"")) {
        // Esc / B after a disc that differs from the original: choose another instead.
        if (!gBusy && !gIso.empty()) {
            log_line("the player went back to choose another disc image");
            gIso.clear();
            post(R"({"type":"reset"})");
        }
    } else if (has(L"\"cmd\":\"restore\"")) {
        if (!gBusy.exchange(true)) {
            restore_backup();
            gBusy = false;
        }
    } else if (has(L"\"cmd\":\"state\"")) {
        size_t a = json.find(L"\"name\":\"");
        if (a != std::wstring::npos) {
            a += 8;
            screenshot(json.substr(a, json.find(L'"', a) - a));
        }
    } else if (has(L"\"cmd\":\"pick\"")) {
        pick();
    } else if (has(L"\"cmd\":\"build\"")) {
        if ((!gIso.empty() || gFromExtracted) && !gBusy.exchange(true)) std::thread(build_thread).detach();
    } else if (has(L"\"cmd\":\"play\"")) {
        if (env(L"SPARKPARK_NO_LAUNCH") == L"1" || launch_game()) PostMessageW(gWnd, WM_CLOSE, 0, 0);
        else MessageBoxW(gWnd, L"The game couldn't be started.", L"SparkPark", MB_ICONERROR);
    }
}

// ------------------------------------------------------------------------------------------
static LRESULT CALLBACK wnd_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        if (gCtl) {
            RECT r;
            GetClientRect(w, &r);
            gCtl->put_Bounds(r);
        }
        return 0;
    case WM_APP_POST: {
        std::unique_ptr<std::wstring> s((std::wstring*)lp);
        if (wp == 1) capture(*s);
        else if (gView) gView->PostWebMessageAsJson(s->c_str());
        return 0;
    }
    case WM_CLOSE:
        if (gBusy) {
            if (MessageBoxW(w, L"SparkPark is still working. Stop and close anyway?", L"SparkPark",
                            MB_YESNO | MB_ICONQUESTION) != IDYES)
                return 0;
            log_line("closed by the player during " + std::string(gIso.empty() && !gFromExtracted ? "the disc check" : "the build"));
            ExitProcess(0);  // the job object ends dtk / Python / clang with us
        }
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

static fs::path choose_home() {
    std::wstring e = env(L"SPARKPARK_HOME");
    if (!e.empty()) return e;
    if (fs::exists(gApp / L"portable.txt")) return gApp / L"data";
    PWSTR local = nullptr;
    fs::path p = SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)) ? fs::path(local) : gApp;
    CoTaskMemFree(local);
    return p / L"SparkPark";
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR args, int) {
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    gApp = fs::path(self).parent_path();
    gHome = choose_home();
    std::error_code ec;
    fs::create_directories(gHome / "logs", ec);

    bool rebuild = (args && wcsstr(args, L"--rebuild")) || (GetKeyState(VK_SHIFT) & 0x8000);
    read_build_json();
    // Built by another SparkPark version (an update unzipped over it): offer the rebuild, from
    // the disc files already extracted when they're all there.
    gUpdate = build_complete() && gBuiltVersion != kVersion;
    if (gUpdate) {
        gFromExtracted = extracted_complete();
        log_line("== SparkPark " + std::string(kVersion) + " found a game built by " +
                 (gBuiltVersion.empty() ? std::string("an unknown version") : gBuiltVersion) +
                 (gFromExtracted ? "; rebuild offered from the extracted disc" : "; the disc files are missing, so the disc is asked for again"));
    }
    if (!rebuild && !gUpdate && build_complete()) {
        if (launch_game()) return 0;
        MessageBoxW(nullptr, L"The game couldn't be started. Run SparkPark with --rebuild to build it again.",
                    L"SparkPark", MB_ICONERROR);
        return 1;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(0x1b, 0x1d, 0x26));
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"SparkParkBuilder";
    RegisterClassW(&wc);
    // 1280x720 at the monitor's scale, shrunk to fit its work area; the page scales to fit.
    UINT dpi = GetDpiForSystem();
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    double s = std::min({dpi / 96.0, (work.right - work.left) * 0.92 / 1280, (work.bottom - work.top) * 0.88 / 720});
    RECT r{0, 0, (LONG)(1280 * s), (LONG)(720 * s)};
    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    gWnd = CreateWindowW(wc.lpszClassName, L"SparkPark", style, work.left + ((work.right - work.left) - ww) / 2,
                         work.top + ((work.bottom - work.top) - wh) / 2, ww, wh, nullptr, nullptr, inst, nullptr);
    ShowWindow(gWnd, SW_SHOW);

    std::wstring userData = (gHome / L"webview2").wstring();
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([](HRESULT res, ICoreWebView2Environment* envp) -> HRESULT {
            if (FAILED(res) || !envp) return res;
            return envp->CreateCoreWebView2Controller(gWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [](HRESULT res2, ICoreWebView2Controller* ctl) -> HRESULT {
                    if (FAILED(res2) || !ctl) return res2;
                    gCtl = ctl;
                    gCtl->get_CoreWebView2(&gView);
                    ComPtr<ICoreWebView2Settings> st;
                    gView->get_Settings(&st);
                    st->put_AreDefaultContextMenusEnabled(FALSE);
                    st->put_AreDevToolsEnabled(env(L"SPARKPARK_DEVTOOLS") == L"1");
                    st->put_IsZoomControlEnabled(FALSE);
                    st->put_IsStatusBarEnabled(FALSE);
                    ComPtr<ICoreWebView2Settings3> st3;
                    if (SUCCEEDED(st.As(&st3))) st3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
                    ComPtr<ICoreWebView2_3> v3;
                    if (SUCCEEDED(gView.As(&v3)))
                        v3->SetVirtualHostNameToFolderMapping(L"sparkpark.local", (gApp / L"ui").c_str(),
                                                              COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
                    std::wstring press = env(L"SPARKPARK_AUTOPRESS");
                    if (!press.empty())
                        gView->AddScriptToExecuteOnDocumentCreated((L"window.SP_AUTOPRESS=" + std::to_wstring(_wtoi(press.c_str())) + L";").c_str(), nullptr);
                    EventRegistrationToken tok;
                    gView->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) -> HRESULT {
                            LPWSTR j = nullptr;
                            if (SUCCEEDED(a->get_WebMessageAsJson(&j)) && j) {
                                on_message(j);
                                CoTaskMemFree(j);
                            }
                            return S_OK;
                        }).Get(), &tok);
                    RECT rc;
                    GetClientRect(gWnd, &rc);
                    gCtl->put_Bounds(rc);
                    gView->Navigate(L"https://sparkpark.local/index.html");
                    return S_OK;
                }).Get());
        }).Get());
    if (FAILED(hr)) {
        MessageBoxW(gWnd,
                    L"SparkPark needs the Microsoft Edge WebView2 Runtime, which comes with Windows 11 and most "
                    L"Windows 10 PCs. Install it from https://go.microsoft.com/fwlink/p/?LinkId=2124703 and run "
                    L"SparkPark again.",
                    L"SparkPark", MB_ICONERROR);
        return 1;
    }
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
