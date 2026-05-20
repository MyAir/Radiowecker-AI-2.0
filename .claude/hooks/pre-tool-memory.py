#!/usr/bin/env python3
"""PreToolUse hook: inject project MEMORY.md on first tool call of this process context."""
import json
import os
import sys
import tempfile
from pathlib import Path


def main():
    # Use parent process ID as session identifier
    # PPID = Claude Code process — stable within a session, new for each subagent
    ppid = os.getppid()
    flag_path = Path(tempfile.gettempdir()) / f"claude-memory-loaded-{ppid}"

    # Already loaded for this process — exit silently (no output = no context injection)
    if flag_path.exists():
        sys.exit(0)

    # Mark as loaded for this process
    flag_path.touch()

    project_dir = os.environ.get('CLAUDE_PROJECT_DIR', os.getcwd())
    project_path = Path(project_dir)

    # Per claude.md: memory lives in-repo at .claude/memory/
    # MEMORY.md = project notes; memory.md = topic index. Some repos use only
    # one of them (and on case-insensitive filesystems they may resolve to the
    # same file), so load each at most once.
    memory_file = project_path / '.claude' / 'memory' / 'MEMORY.md'
    global_idx = project_path / '.claude' / 'memory' / 'memory.md'

    parts = []
    seen = set()

    def load(path: Path, header: str, line_limit: int | None = None) -> None:
        try:
            key = path.resolve().as_posix().lower()
        except OSError:
            key = str(path).lower()
        if key in seen:
            return
        if not path.exists():
            return
        seen.add(key)
        text = path.read_text(encoding='utf-8')
        if line_limit is not None:
            text = '\n'.join(text.splitlines()[:line_limit])
        parts.append(f"=== {header} ===\n{text}")

    load(memory_file, f"Project Memory: {project_dir}", line_limit=200)
    load(global_idx, "Memory Index")

    if not parts:
        parts.append(f"(no project memory found at {memory_file} or {global_idx})")

    context = '\n\n'.join(parts)

    output = {
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "additionalContext": context
        }
    }

    print(json.dumps(output))
    sys.exit(0)


if __name__ == "__main__":
    main()
