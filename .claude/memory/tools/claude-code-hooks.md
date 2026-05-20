# Tools: Claude Code Hooks

## 2026-05-21 — PreToolUse Memory Injection Hook

Auto-injects `.claude/memory/MEMORY.md` (project notes, first 200 lines) and
`.claude/memory/memory.md` (topic index) into the model's context on the first
tool call of each Claude process. Subsequent tool calls in the same session are
no-ops (~5 ms shell overhead).

### Files
- `.claude/settings.json` — registers a `PreToolUse` hook with `matcher: "*"`,
  timeout 5 s, command:
  `"C:\Program Files\Git\bin\bash.exe" .claude/hooks/pre-tool-memory.sh`
- `.claude/hooks/pre-tool-memory.sh` — fast PPID-flag check; if not yet loaded,
  exec the Python script. Uses `$TMPDIR` / `$TEMP` / `$TMP` / `/tmp`. Probes
  `python3 -c ""` then falls back to `python -c ""` (skips the Microsoft Store
  `python3.exe` stub).
- `.claude/hooks/pre-tool-memory.py` — reads both files, dedups by
  `Path.resolve().as_posix().lower()` (case-insensitive Windows FS would
  otherwise double-load `MEMORY.md`/`memory.md`), emits
  `{"hookSpecificOutput":{"hookEventName":"PreToolUse","additionalContext":...}}`.

### One-shot per session via PPID flag
```bash
TMP="${TMPDIR:-${TEMP:-${TMP:-/tmp}}}"
FLAG="$TMP/claude-memory-loaded-$PPID"
[ -f "$FLAG" ] && exit 0
```
PPID = the Claude Code process. Stable within a session, distinct per subagent.
On the Python side: `flag_path.touch()` before emitting context.

### Windows gotchas
- `bash` is **not** on PATH. Hard-code the absolute path
  `C:\Program Files\Git\bin\bash.exe` in `settings.json`.
- Microsoft Store ships a `python3.exe` stub that opens the Store instead of
  running code. Probe with `python3 -c ""` (exit 0 = real interpreter) before
  using it; fall back to `python`.
- Git Bash maps `$TEMP` from Windows env, so the flag lands in
  `C:\Users\<user>\AppData\Local\Temp\claude-memory-loaded-<ppid>`. Clear it
  manually when iterating on the hook:
  `Remove-Item "$env:TEMP\claude-memory-loaded-*"`.

### Smoke test
```powershell
Remove-Item "$env:TEMP\claude-memory-loaded-*" -ErrorAction SilentlyContinue
& "C:\Program Files\Git\bin\bash.exe" .claude/hooks/pre-tool-memory.sh `
  | python -c "import sys,json;d=json.loads(sys.stdin.read());print(len(d['hookSpecificOutput']['additionalContext']))"
```
Expected: a number > 0 (currently ~1.7 KB). Empty output = flag was already
present (delete it) or no memory files exist.

### Path convention
Per `claude.md`, memory lives **in-repo** at `.claude/memory/` — NOT under
`~/.claude/projects/<mapped>/memory/`. The script previously looked under the
user-home path; that was a bug fixed 2026-05-21 along with the dedup.
