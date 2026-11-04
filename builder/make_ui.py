"""Make the builder's page (builder/ui/index.html) from the design reference.

docs/launcher/sparkpark-launcher.html is the single source for the look and animations. This
applies the few changes the real builder needs and nothing else:
  - the Google Fonts link -> a local @font-face (fonts/Baloo2-wght.ttf, SIL OFL; never loaded
    from the internet);
  - the stage scales to fit the window (body fills the window, the 1280x720 stage gets an id);
  - the "Build Recomp" label gets an id, so a failed build can offer "Retry Build";
  - the timer-driven script -> builder/ui_script.js, wired to the launcher.

  python builder/make_ui.py
"""
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "docs", "launcher", "sparkpark-launcher.html")
OUT = os.path.join(HERE, "ui")
FONT_FACE = """<style>
@font-face{font-family:'Baloo 2';src:url('fonts/Baloo2-wght.ttf') format('truetype');font-weight:400 800;font-display:block}
html,body{height:100%;overflow:hidden}
body{min-height:0}
#spStage{flex:none;transform-origin:50% 50%}
</style>"""


def replace_once(text: str, old: str, new: str, what: str) -> str:
    if text.count(old) != 1:
        sys.exit(f"make_ui: expected exactly one {what} in the design reference, found {text.count(old)}")
    return text.replace(old, new)


def main() -> int:
    html = open(SRC, encoding="utf-8").read()
    html = re.sub(r'<link rel="preconnect"[^>]*>\s*', "", html)
    html, n = re.subn(r'<link href="https://fonts\.googleapis\.com[^>]*>', FONT_FACE, html)
    if n != 1:
        sys.exit("make_ui: the Google Fonts link wasn't found")
    html = replace_once(html, "<title>SparkPark Builder — design reference</title>", "<title>SparkPark</title>", "title")
    html = replace_once(html, '<div style="width: 1280px; height: 720px; position: relative;',
                        '<div id="spStage" style="width: 1280px; height: 720px; position: relative;', "stage div")
    html = replace_once(html, "<span>Build Recomp</span>", '<span id="buildLabel">Build Recomp</span>', "Build label")
    script = open(os.path.join(HERE, "ui_script.js"), encoding="utf-8").read()
    html, n = re.subn(r"<script>.*?</script>", lambda _: "<script>\n" + script + "</script>", html, flags=re.S)
    if n != 1:
        sys.exit("make_ui: expected one <script> block")
    if "googleapis" in html or "http://" in html or "https://" in html.replace("https://sparkpark.local", ""):
        sys.exit("make_ui: the page still refers to the internet")
    os.makedirs(os.path.join(OUT, "fonts"), exist_ok=True)
    with open(os.path.join(OUT, "index.html"), "w", encoding="utf-8", newline="\n") as f:
        f.write(html)
    for name in ("Baloo2-wght.ttf", "OFL.txt"):
        shutil.copy2(os.path.join(HERE, "fonts", name), os.path.join(OUT, "fonts", name))
    print(f"wrote {os.path.join(OUT, 'index.html')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
