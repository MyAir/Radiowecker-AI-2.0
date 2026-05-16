#!/bin/bash
# Shell wrapper for pre-tool-memory.py
# Checks the PPID flag before invoking Python — ~5ms overhead vs ~80ms for Python startup
FLAG="/tmp/claude-memory-loaded-$(ps -o ppid= -p $$ | tr -d ' ')"
[ -f "$FLAG" ] && exit 0
[ -f .claude/hooks/pre-tool-memory.py ] || exit 0
exec python3 .claude/hooks/pre-tool-memory.py
