#!/bin/bash
# Shell wrapper for pre-tool-memory.py
# Checks the PPID flag before invoking Python — ~5ms overhead vs ~80ms for Python startup.
# Cross-platform: uses $TMPDIR when set (Git Bash on Windows, macOS) and falls
# back to /tmp on Linux. Tries python3 first, then python (Git Bash on Windows
# typically only ships `python`).
TMP="${TMPDIR:-${TEMP:-${TMP:-/tmp}}}"
FLAG="$TMP/claude-memory-loaded-$PPID"
[ -f "$FLAG" ] && exit 0
[ -f .claude/hooks/pre-tool-memory.py ] || exit 0
# Probe interpreters with `-c ""` so the Microsoft Store python3.exe stub
# (which launches the Store rather than running code) is rejected.
if python3 -c "" >/dev/null 2>&1; then
    exec python3 .claude/hooks/pre-tool-memory.py
elif python -c "" >/dev/null 2>&1; then
    exec python .claude/hooks/pre-tool-memory.py
fi
