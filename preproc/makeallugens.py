"""build allugens.py and allugens.srp

for .py, pull all imports in a file to the top and do each one once

The command line is
    python3 makeallugens.py files-with-srp-suffix

For Python, any line beginning with "from" or "import" is
removed from the file and moved to the top
after removing all duplicates.
"""

import sys
from pathlib import Path


def consolidate_imports(lines) -> None:
    """Move all lines beginning with import/from to the top of the file.

    Import lines are deduplicated, preserving first-seen order.
    All other lines remain in their original relative order.
    """
    imports = []
    seen = set()
    body = []

    for line in lines:
        stripped = line.lstrip()
        if stripped.startswith("import ") or stripped.startswith("from "):
            key = stripped.rstrip("\n")
            if key not in seen:
                seen.add(key)
                imports.append(line if line.endswith("\n") else line + "\n")
        else:
            body.append(line if line.endswith("\n") else line + "\n")

    if imports and body:
        new_text = "".join(imports) + "\n" + "".join(body)
    else:
        new_text = "".join(imports + body)

    return new_text


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: python3 makeallugens.py list_of_file_names_with_srp_ext")
        return 1

    files = sys.argv[1 : ]

    # do allugens.srp first
    lines = ["# allugens.srp -- " +
             "consolidated UGen subclasses based on dspmanifest.txt\n\n"]
    for file in files:
        target = Path(file)
        if not target.exists() or not target.is_file():
            print(f"Error: cannot open file: {target}")
            return 1
        lines.append("\n#---- included from " + file + " ----\n\n")
        lines.append(target.read_text())
    srp_out = Path("allugens.srp")
    srp_out.write_text("".join(lines))

    # do allugens.py
    lines = ["# allugens.py -- " +
             "consolidated UGen subclasses based on dspmanifest.txt\n\n"]
    for file in files:
        # replace files that contain /arco/serpent/srp/ with /arco/pyarco/ugens/
        # which is where non-fause .py sources live:
        file = file.replace("/arco/serpent/srp/", "/arco/pyarco/ugens/")
        target = Path(file).with_suffix(".py")
        if not target.exists() or not target.is_file():
            print(f"Error: cannot open file: {target}")
            return 1
        lines.append("\n#---- included from " + str(target) + " ----\n\n")
        lines += target.read_text().splitlines(keepends=True)
    lines = consolidate_imports(lines)
    py_out = Path("allugens.py")
    py_out.write_text("".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

