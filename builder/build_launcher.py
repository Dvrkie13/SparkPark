"""Build the SparkPark launcher (SparkPark.exe) with MSVC, on the developer's PC.

The launcher contains no game code: it is the WebView2 host for builder/ui and runs the build
steps. It links the WebView2 loader and the C runtime statically (/MT), so a player's PC needs
neither WebView2Loader.dll nor a Visual C++ redistributable, only the WebView2 Runtime.

  python builder/build_launcher.py --webview2 D:\\Tools\\webview2\\1.0.4314.35 --out <dir>
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "recomp"))
from build import vcvars  # noqa: E402  (upstream's MSVC finder)

MANIFEST = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <dependency><dependentAssembly>
    <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0"
                      processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/>
  </dependentAssembly></dependency>
  <application xmlns="urn:schemas-microsoft-com:asm.v3"><windowsSettings>
    <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
    <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
  </windowsSettings></application>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1"><application>
    <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
  </application></compatibility>
</assembly>
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--webview2", required=True, help="the unpacked Microsoft.Web.WebView2 NuGet package")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    obj = os.path.join(a.out, "obj")
    os.makedirs(obj, exist_ok=True)
    manifest = os.path.join(obj, "launcher.manifest")
    with open(manifest, "w", newline="\n") as f:
        f.write(MANIFEST)
    native = os.path.join(a.webview2, "build", "native")
    exe = os.path.join(a.out, "SparkPark.exe")
    bat = os.path.join(obj, "build.bat")
    with open(bat, "w") as f:
        f.write(f'@call "{vcvars()}" >nul 2>nul\n')
        f.write(f'cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Fo"{obj}\\\\" '
                f'/I"{os.path.join(native, "include")}" /I"{os.path.join(ROOT, "recomp", "boot")}" '
                f'/I"{os.path.join(ROOT, "recomp", "runtime")}" '
                f'"{os.path.join(HERE, "launcher.cpp")}" "{os.path.join(ROOT, "recomp", "boot", "disc_verify.c")}" '
                f'/Fe"{exe}" /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:"{manifest}" '
                f'"{os.path.join(native, "x64", "WebView2LoaderStatic.lib")}" '
                f'user32.lib gdi32.lib ole32.lib shell32.lib shlwapi.lib comdlg32.lib advapi32.lib version.lib '
                f'|| exit /b 1\n')
    subprocess.check_call(["cmd", "/c", bat])
    print(f"launcher: {exe} ({os.path.getsize(exe) / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
