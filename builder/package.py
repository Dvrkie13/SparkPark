"""Assemble the SparkPark app folder: what goes in the release zip.

Contains no game code and nothing from a disc: the launcher, its page and font, the runtime as a
library built from a disc-free header, the recompiler scripts with the public function list,
embedded Python, a trimmed llvm-mingw, decomp-toolkit and the button prompt icons.

  python builder/package.py stage --out <dir> --launcher <SparkPark.exe> --llvm <llvm-mingw dir>
                                  --python <python embeddable dir> --dtk <dtk.exe>
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# What a player's build needs from llvm-mingw (x86_64 only); see docs/PROGRESS.md, phase 1.
LLVM_BIN = ["clang.exe", "clang-23.exe", "ld.lld.exe", "libLLVM-23.dll", "libclang-cpp.dll", "libc++.dll",
            "libunwind.dll", "mingw32-common.cfg", "x86_64-w64-windows-gnu.cfg", "x86_64-pc-windows-gnu.cfg"]
LLVM_RT_SKIP = ("asan", "ubsan", "fuzzer", "xray", "profile", "memprof", "tsan", "hwasan", "orc_rt", "stats")


def copytree(src, dst, ignore=None):
    shutil.copytree(src, dst, dirs_exist_ok=True, ignore=ignore)


def stage_llvm(src, dst):
    os.makedirs(os.path.join(dst, "bin"), exist_ok=True)
    for f in LLVM_BIN:
        shutil.copy2(os.path.join(src, "bin", f), os.path.join(dst, "bin", f))
    shutil.copy2(os.path.join(src, "LICENSE.TXT"), dst)
    res = glob.glob(os.path.join(src, "lib", "clang", "*"))[0]
    rel = os.path.relpath(res, src)
    copytree(os.path.join(res, "include"), os.path.join(dst, rel, "include"))
    os.makedirs(os.path.join(dst, rel, "lib", "windows"), exist_ok=True)
    for f in glob.glob(os.path.join(res, "lib", "windows", "*x86_64*")):
        if not any(s in os.path.basename(f) for s in LLVM_RT_SKIP):
            shutil.copy2(f, os.path.join(dst, rel, "lib", "windows"))
    copytree(os.path.join(src, "x86_64-w64-mingw32", "lib"), os.path.join(dst, "x86_64-w64-mingw32", "lib"))
    copytree(os.path.join(src, "include"), os.path.join(dst, "include"), shutil.ignore_patterns("c++"))


def stage(a):
    out = a.out
    if os.path.exists(out):
        sys.exit(f"{out} exists; remove it first")
    os.makedirs(out)
    subprocess.check_call([sys.executable, os.path.join(HERE, "make_ui.py")])
    shutil.copy2(a.launcher, os.path.join(out, "SparkPark.exe"))
    copytree(os.path.join(HERE, "ui"), os.path.join(out, "ui"))
    os.makedirs(os.path.join(out, "tools"))
    shutil.copy2(a.dtk, os.path.join(out, "tools", "dtk.exe"))
    copytree(os.path.join(ROOT, "recomp", "assets", "prompts"), os.path.join(out, "prompts"))
    # The recompiler: upstream's scripts, the public function list and the hook list, plus the
    # runtime headers the generated C includes, laid out as in the repository.
    rc = os.path.join(out, "recompiler")
    for f in glob.glob(os.path.join(ROOT, "tools", "recomp", "*.py")):
        if os.path.basename(f) != "build.py":
            os.makedirs(os.path.join(rc, "tools", "recomp"), exist_ok=True)
            shutil.copy2(f, os.path.join(rc, "tools", "recomp"))
    os.makedirs(os.path.join(rc, "tools", "sparkpark"))
    shutil.copy2(os.path.join(ROOT, "tools", "sparkpark", "build_player.py"), os.path.join(rc, "tools", "sparkpark"))
    os.makedirs(os.path.join(rc, "config", "R8AE01"))
    shutil.copy2(os.path.join(ROOT, "config", "R8AE01", "symbols.txt"), os.path.join(rc, "config", "R8AE01"))
    os.makedirs(os.path.join(rc, "recomp", "patches"))
    shutil.copy2(os.path.join(ROOT, "recomp", "patches", "hooks.txt"), os.path.join(rc, "recomp", "patches"))
    os.makedirs(os.path.join(rc, "recomp", "runtime"))
    for f in glob.glob(os.path.join(ROOT, "recomp", "runtime", "*.h")):
        shutil.copy2(f, os.path.join(rc, "recomp", "runtime"))
    stage_llvm(a.llvm, os.path.join(out, "toolchain", "llvm-mingw"))
    copytree(a.python, os.path.join(out, "toolchain", "python"))
    # The runtime library, built from the disc-free header with the full llvm-mingw of the same
    # release (it also needs llvm-ar and llvm-windres, which players don't).
    rt_tmp = os.path.join(os.path.dirname(out), os.path.basename(out) + "-runtime-build")
    shutil.rmtree(rt_tmp, ignore_errors=True)
    env = dict(os.environ, SPARKPARK_LLVM=a.llvm)
    subprocess.check_call([sys.executable, os.path.join(ROOT, "tools", "sparkpark", "build_player.py"), "runtime",
                           "--out", rt_tmp], env=env)
    os.makedirs(os.path.join(out, "runtime"))
    for f in ("libsparkpark_runtime.a", "app_manifest.o"):
        shutil.copy2(os.path.join(rt_tmp, f), os.path.join(out, "runtime"))
    shutil.rmtree(rt_tmp, ignore_errors=True)
    total = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(out) for f in fs)
    print(f"staged {out}: {total / 1e6:.1f} MB")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("stage")
    s.add_argument("--out", required=True)
    s.add_argument("--launcher", required=True)
    s.add_argument("--llvm", required=True)
    s.add_argument("--python", required=True)
    s.add_argument("--dtk", required=True)
    a = ap.parse_args()
    return {"stage": stage}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
