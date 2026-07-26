import sys
import os
import time

sys.path.insert(0, os.path.abspath("."))
sys.path.insert(0, os.path.abspath("_themes"))
extensions = [
    "hawkmoth",
    "daslang",
]

# -- Hawkmoth configuration -------------------------------------------------
# Hawkmoth parses C/C++ source directly with libclang (no Doxygen needed).
_this_dir = os.path.dirname(os.path.abspath(__file__))
_project_root = os.path.normpath(os.path.join(_this_dir, "..", ".."))

hawkmoth_root = os.path.join(_project_root, "src")
hawkmoth_clang = [
    f"-I{os.path.join(_project_root, 'src')}",
    f"-I{os.path.join(_project_root, 'src', 'wsl')}",
    f"-I{os.path.join(_project_root, 'build', '_deps', 'entt-src', 'single_include')}",
    f"-I{os.path.join(_project_root, 'build', '_deps', 'daslang-src', 'include')}",
    f"-I{os.path.join(_project_root, 'build', '_deps', 'daslang-build', 'include')}",
    "-std=c++20",
    "-D__clang__",
    "-Wno-everything",
    "-D__cpp_exceptions=1",
    # GCC internal include path (stddef.h, float.h, etc.)
    "-isystem/usr/lib/gcc/x86_64-pc-linux-gnu/16.1.1/include",
    # System C++ standard library paths
    "-isystem/usr/include/c++/16.1.1",
    "-isystem/usr/include/c++/16.1.1/x86_64-pc-linux-gnu",
    "-isystem/usr/include/c++/16.1.1/backward",
    "-isystem/usr/lib/clang/21/include",
    "-isystem/usr/local/include",
    "-isystem/usr/include",
]

templates_path = ["_templates"]
suppress_warnings = ["toctree.not_included"]
source_suffix = ".rst"
master_doc = "index"

project = "Weasel Engine"
copyright = "2024-%s" % time.strftime("%Y")
author = "Weasel Contributors"

version = "0.1.0"
release = "0.1.0"

language = "en"
exclude_patterns = ["_build"]
pygments_style = "sphinx"
highlight_language = "cpp"
primary_domain = "cpp"
todo_include_todos = False

html_theme = "weasel"
html_theme_path = [os.path.abspath("_themes")]
html_theme_options = {}
html_logo = "_static/logo.svg"
html_favicon = "_static/logo.svg"
html_static_path = ["_static"]
html_sidebars = {
    "**": ["logo.html", "searchbox.html", "globaltoc.html", "sourcelink.html"]
}
htmlhelp_basename = "weasel_doc"

latex_documents = [
    (master_doc, "weasel.tex", "Weasel Engine Documentation", author, "manual"),
]
