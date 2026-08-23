"""Custom Pygments style for the Weasel documentation, built from the
Weasel palette.

Palette
-------
primary    #68763c  dusty_olive
secondary  #dec16b  old_gold
bg1        #09090c  black
bg2        #101115  onyx
fg         #f2f2f2  warm white
gray       #454551  gray
dark       #1e202c  dark gray
"""

from pygments.style import Style
from pygments.token import (
    Comment,
    Error,
    Generic,
    Keyword,
    Literal,
    Name,
    Number,
    Operator,
    Punctuation,
    String,
    Text,
    Whitespace,
)


class WeaselStyle(Style):
    default_style = ""
    background_color = "#101115"  # --wsl-bg2 (onyx)
    highlight_color = "#1e202c"  # --wsl-dark

    styles = {
        Text: "#f2f2f2",  # --wsl-fg
        Whitespace: "#f2f2f2",
        Comment: "#454551 italic",  # --wsl-gray
        Comment.Preproc: "#dec16b",  # --wsl-secondary
        Comment.Special: "#dec16b bold",
        Keyword: "#68763c bold",  # --wsl-primary
        Keyword.Type: "#dec16b",  # --wsl-secondary
        Keyword.Constant: "#dec16b",
        Keyword.Declaration: "#68763c bold",
        Keyword.Namespace: "#68763c bold",
        Keyword.Pseudo: "#68763c",
        Literal: "#f2f2f2",
        String: "#dec16b",  # --wsl-secondary
        String.Doc: "#454551 italic",
        String.Char: "#dec16b",
        String.Escape: "#68763c",
        String.Regex: "#dec16b",
        Number: "#68763c",  # --wsl-primary
        Number.Float: "#68763c",
        Number.Hex: "#68763c",
        Number.Integer: "#68763c",
        Number.Oct: "#68763c",
        Operator: "#f2f2f2",
        Operator.Word: "#68763c bold",
        Punctuation: "#f2f2f2",
        Name: "#f2f2f2",
        Name.Tag: "#68763c",
        Name.Attribute: "#f2f2f2",
        Name.Builtin: "#dec16b",  # --wsl-secondary
        Name.Builtin.Pseudo: "#dec16b",
        Name.Class: "#68763c bold",  # --wsl-primary
        Name.Function: "#dec16b bold",  # --wsl-secondary
        Name.Namespace: "#68763c",
        Name.Variable: "#f2f2f2",
        Name.Variable.Class: "#f2f2f2",
        Name.Variable.Global: "#f2f2f2",
        Name.Variable.Instance: "#f2f2f2",
        Name.Constant: "#68763c",
        Name.Entity: "#dec16b",
        Name.Exception: "#dec16b bold",
        Name.Label: "#68763c",
        Name.Decorator: "#dec16b",
        Generic: "#f2f2f2",
        Generic.Deleted: "#dec16b",
        Generic.Emph: "italic",
        Generic.Error: "#dec16b bold",
        Generic.Heading: "#68763c bold",
        Generic.Inserted: "#68763c",
        Generic.Output: "#454551",
        Generic.Prompt: "#454551",
        Generic.Strong: "bold",
        Generic.Subheading: "#68763c bold",
        Generic.Traceback: "#454551",
        Error: "#dec16b bold",
    }
