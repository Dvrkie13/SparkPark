// SparkPark launcher: the one-time builder (CLAUDE.md, "Launcher (one-time builder)").
//
// What the player runs. If a completed build exists it starts the game and exits; otherwise it
// shows the builder page (ui/index.html, made from the design reference by make_ui.py) in a
// WebView2 window and runs the real steps behind its button:
//   pick  -> file picker -> "dtk disc info" + "dtk disc verify", classified by disc_verify.c
//   build -> extract (dtk) -> recompile (embedded Python, tools/recomp/recomp.py)
//            -> compile + link (llvm-mingw, tools/sparkpark/build_player.py)
//   play  -> start the game, close the builder.
//
// Everything built from the player's disc stays on the player's PC, in SPARKPARK_HOME:
//   %LOCALAPPDATA%\SparkPark            by default;
//   <launcher folder>\data              when portable.txt sits beside the launcher;
//   %SPARKPARK_HOME%                    when set.
// Inside it: game\SparkParkGame.exe, game\R8AE01\{disc,sys,nand}, game\settings.ini,
// build.json (the "build complete" marker), logs\build.log, work\ (deleted after a build).
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

#include "WebView2.h"
#include "disc_verify.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

static const char kVersion[] = "0.2.0-alpha";
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
    BOOL ok = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                             envblock.data(), cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) {
        CloseHandle(rd);
        log_line("  (could not start)");
        return -1;
    }
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
    while (WaitForSingleObject(pi.hProcess, 200) == WAIT_TIMEOUT) {
        drain();
        if (tick) tick();
    }
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
static void verify_thread(std::wstring iso) {
    post(R"({"type":"verifying"})");
    disc_file file{};
    if (FILE* f = _wfopen(iso.c_str(), L"rb")) {
        if (_fseeki64(f, 0x200, SEEK_SET) == 0) fread(file.magic200, 1, 4, f);
        _fseeki64(f, 0, SEEK_END);
        file.size = _ftelli64(f);
        fclose(f);
    }
    fs::path dtk = gApp / "tools" / "dtk.exe";
    std::string info, verify;
    char msg[512] = "";
    log_line("== disc check: " + narrow(iso));
    int rc = run(q(dtk) + L" disc info " + q(iso), {}, {}, nullptr, nullptr, &info);
    disc_result r;
    if (rc == -1) {
        snprintf(msg, sizeof msg, "Couldn't start tools\\dtk.exe, which reads disc images. Is the SparkPark folder complete?");
        r = DISC_UNREADABLE;
    } else if ((r = disc_check_info(info.c_str(), &file, msg, sizeof msg)) == DISC_OK) {
        run(q(dtk) + L" disc verify " + q(iso), {}, {}, nullptr, nullptr, &verify);
        r = disc_check_verify(info.c_str(), verify.c_str(), &file, msg, sizeof msg);
    }
    log_line(r == DISC_OK ? "disc OK" : std::string("disc rejected: ") + msg);
    if (r == DISC_OK) {
        gIso = iso;
        post(R"({"type":"verified"})");
    } else {
        // The sign has room for about two lines; the full explanation is in the log.
        std::string why;
        switch (r) {
        case DISC_WRONG_GAME: why = "That's a different game. SparkPark needs PokePark Wii (USA)."; break;
        case DISC_WRONG_REGION: {
            std::string m = msg, which = "another region's release";
            size_t a = m.find("That's "), b = m.find(" of PokePark");
            if (a == 0 && b != std::string::npos) which = m.substr(7, b - 7);
            why = "Wrong region: that's " + which + ". SparkPark needs the USA disc (R8AE01).";
            break;
        }
        case DISC_WRONG_REVISION: why = "Wrong revision: SparkPark needs the first release (Rev 0) of the USA disc."; break;
        case DISC_NKIT_SCRUBBED: why = "That's an NKit or scrubbed image. SparkPark needs a full 1:1 dump."; break;
        case DISC_MODIFIED: why = "That image is modified: it doesn't match the original disc. Use a clean dump."; break;
        default: why = rc == -1 ? msg : "That file couldn't be read as a disc image. Pick an ISO, RVZ or WBFS."; break;
        }
        post(std::string(R"({"type":"discError","reason":)") + json_str(why) + "}");
    }
    gBusy = false;
}

// ------------------------------------------------------------------------------------------
// The build.
static void progress(double pct, const char* stage) {
    char buf[96];
    snprintf(buf, sizeof buf, R"({"type":"progress","pct":%.1f,"stage":"%s"})", pct, stage);
    post(buf);
}

static bool build_steps(std::string& why) {
    std::error_code ec;
    const fs::path game = gHome / "game", r8 = game / "R8AE01", disc = r8 / "disc", sys = r8 / "sys";
    const fs::path work = gHome / "work", gen = work / "gen";
    const fs::path recompiler = gApp / "recompiler", python = gApp / "toolchain" / "python" / "python.exe";
    fs::path llvm = gApp / "toolchain" / "llvm-mingw";
    if (env(L"SPARKPARK_TEST_FAIL") == L"compile") llvm = gApp / "toolchain" / "missing-compiler";
    fs::remove(gHome / "build.json", ec);
    fs::remove_all(work, ec);
    fs::create_directories(gen, ec);

    // 1. Extract (0-12%). The saves in R8AE01\nand are kept.
    progress(0, "extract");
    fs::remove_all(disc, ec);
    fs::create_directories(sys, ec);
    int rc = run(q(gApp / "tools" / "dtk.exe") + L" disc extract -q " + q(gIso) + L" " + q(disc), {}, {}, nullptr,
                 [&] { progress(12.0 * std::min(1.0, (double)tree_bytes(disc) / kExtractBytes), "extract"); });
    if (rc != 0 || !fs::exists(disc / "sys" / "main.dol")) {
        why = "Extracting the disc failed. Is there enough free space? (about 2 GB)";
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

    // The "build complete" marker; later starts go straight into the game.
    std::ofstream(gHome / "build.json", std::ios::binary)
        << "{\n  \"version\": \"" << kVersion << "\",\n  \"disc\": \"R8AE01 Rev 0\",\n  \"disc_sha1\": \""
        << DISC_REDUMP_SHA1 << "\",\n  \"game\": \"game\\\\SparkParkGame.exe\"\n}\n";
    fs::remove_all(work, ec);
    progress(100, "link");
    return true;
}

static void build_thread() {
    log_line("== build, SparkPark " + std::string(kVersion));
    std::string why;
    auto t0 = std::chrono::steady_clock::now();
    bool ok = build_steps(why);
    log_line(ok ? "build complete in " +
                      std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count()) + " s"
                : "build failed: " + why);
    if (ok) post(R"({"type":"done"})");
    else post(std::string(R"({"type":"buildError","reason":)") + json_str(why + " See logs\\build.log.") + "}");
    gBusy = false;
}

// ------------------------------------------------------------------------------------------
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

static void on_message(const std::wstring& json) {
    auto has = [&](const wchar_t* s) { return json.find(s) != std::wstring::npos; };
    if (has(L"\"cmd\":\"state\"")) {
        size_t a = json.find(L"\"name\":\"");
        if (a != std::wstring::npos) {
            a += 8;
            screenshot(json.substr(a, json.find(L'"', a) - a));
        }
    } else if (has(L"\"cmd\":\"pick\"")) {
        pick();
    } else if (has(L"\"cmd\":\"build\"")) {
        if (!gIso.empty() && !gBusy.exchange(true)) std::thread(build_thread).detach();
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
            ExitProcess(0);  // the build's child processes end with the pipe
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
    if (!rebuild && build_complete()) {
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
