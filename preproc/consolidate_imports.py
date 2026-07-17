"""pull all imports in a file to the top and do each one once

The command line is
    python3 consolidate_imports.py file_name
where file_name is the file to process.

Any line beginning with "from" or "import" is
removed from the file and moved to the top
after removing all duplicates.
"""

import sys
from pathlib import Path


def consolidate_imports(path: Path) -> None:
    """Move all lines beginning with import/from to the top of the file.

    Import lines are deduplicated, preserving first-seen order.
    All other lines remain in their original relative order.
    """
    original = path.read_text()
    lines = original.splitlines(keepends=True)

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

    if new_text != original:
        path.write_text(new_text)


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python3 consolidate_imports.py file_name")
        return 1

    target = Path(sys.argv[1])
    if not target.exists() or not target.is_file():
        print(f"Error: cannot open file: {target}")
        return 1

    consolidate_imports(target)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

