Add automatic memory injection to Claude Code. Run in plan mode, show me the plan, then execute.

## Before starting

Create tasks for each step below using TaskCreate, then mark each in_progress before starting it and completed when done:

1. Create PreToolUse memory hook files
2. Register hook in settings.json
3. Update ## Global Memory section in CLAUDE.md to reference the hook

**Rule: if any file already exists and would be modified or removed, use AskUserQuestion first. Show the current content and the proposed change. Do not modify without explicit confirmation.**

---

## 1. PreToolUse memory hook files

Create two files in `.claude/hooks/` (create the directory if it doesn't exist). **Create the `.py` file first, then the `.sh` file** — the wrapper calls the Python script, so it needs to exist before the hook is active.

`.claude/hooks/pre-tool-memory.py` — the memory injector (create this first):

```python
#!/usr/bin/env python3
"""PreToolUse hook: inject project MEMORY.md on first tool call of this process context."""
import json
import os
import sys
from pathlib import Path


def main():
    # Use parent process ID as session identifier
    # PPID = Claude Code process — stable within a session, new for each subagent
    ppid = os.getppid()
    flag_path = Path(f"/tmp/claude-memory-loaded-{ppid}")

    # Already loaded for this process — exit silently (no output = no context injection)
    if flag_path.exists():
        sys.exit(0)

    # Mark as loaded for this process
    flag_path.touch()

    project_dir = os.environ.get('CLAUDE_PROJECT_DIR', os.getcwd())

    # Map project dir to .claude/projects key
    # /Users/you/Projects/foo -> -Users-you-Projects-foo
    # Replace / and . with -, keep the leading - (don't lstrip)
    mapped = project_dir.replace('/', '-').replace('.', '-')

    home = Path.home()
    memory_file = home / '.claude' / 'projects' / mapped / 'memory' / 'MEMORY.md'
    global_idx = home / '.claude' / 'memory' / 'memory.md'

    parts = []

    if memory_file.exists():
        lines = memory_file.read_text().splitlines()[:200]
        parts.append(f"=== Project Memory: {project_dir} ===\n" + '\n'.join(lines))
    else:
        parts.append(f"(no project MEMORY.md at {memory_file})")

    if global_idx.exists():
        parts.append("=== Global Memory Index ===\n" + global_idx.read_text())

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
```

`.claude/hooks/pre-tool-memory.sh` — the shell wrapper (create this second):

```bash
#!/bin/bash
# Shell wrapper for pre-tool-memory.py
# Checks the PPID flag before invoking Python — ~5ms overhead vs ~80ms for Python startup
FLAG="/tmp/claude-memory-loaded-$(ps -o ppid= -p $$ | tr -d ' ')"
[ -f "$FLAG" ] && exit 0
[ -f .claude/hooks/pre-tool-memory.py ] || exit 0
exec python3 .claude/hooks/pre-tool-memory.py
```

Make both files executable:

```bash
chmod +x .claude/hooks/pre-tool-memory.sh .claude/hooks/pre-tool-memory.py
```

---

## 2. Register the hook in settings.json

Read `.claude/settings.json`. If a `PreToolUse` key already exists inside `hooks`, add a new entry to it. If it does not exist, add the following block inside the `hooks` object (create `hooks` if it doesn't exist):

```json
"PreToolUse": [
  {
    "matcher": "*",
    "hooks": [
      {
        "type": "command",
        "command": "bash .claude/hooks/pre-tool-memory.sh",
        "timeout": 5
      }
    ]
  }
]
```

---

## 3. Update ## Global Memory in CLAUDE.md

Read `.claude/CLAUDE.md`. Find the `## Global Memory` section. Replace its description line with the following (leave the topic file list unchanged):

Replace this line:
```
Read .claude/memory/memory.md at session start. Load specific topic files only when relevant.
```

With:
```
Project MEMORY.md and this index are auto-injected before each tool call via PreToolUse hook
(.claude/hooks/pre-tool-memory.sh). Load specific topic files only when relevant.
```