Set up a structured, persistent memory management system for Claude Code. Run in plan mode, show me the plan, then execute.

## Before starting

Create tasks for each step below using TaskCreate, then mark each in_progress before starting it and completed when done:

1. Create global memory directory structure
2. Update CLAUDE.md
3. Initialise project MEMORY.md files

**Rule: if any file already exists and would be modified or removed, use AskUserQuestion first. Show the current content and the proposed change. Do not modify without explicit confirmation.**

---

## 1. Global memory directory structure

Create `.claude/memory/` with the following files if they do not already exist:

`.claude/memory/memory.md` — the index file:

```
# Memory Index

Read this file at session start. Load specific topic files only when relevant.

| File | Description | Last updated |
|------|-------------|--------------|
| `general.md` | Cross-project conventions and preferences | {today} |

## Cross-Memory Sync Rule

At session start, after reading this file:
1. Note the Last updated dates in the table above
2. Check projects.md (if it exists) for active project MEMORY.md paths
3. If any project MEMORY.md has content worth promoting to a global tools/ or domain/ file, flag it
4. Update the Last updated date on this file after any changes

## Domain Knowledge Lifecycle

1. Staging — knowledge accumulates in domain/{name}/
2. Promotion — enough knowledge exists to package as a plugin/skill
3. Pointer — after promotion, the memory file becomes a pointer to the plugin
```

`.claude/memory/general.md` — the cross-project conventions file:

```
# General - Cross-Project Conventions

## Writing & Naming Conventions

(Populate as you go — add things like: preferred date formats, naming patterns, style preferences)

## Workflow Preferences

(Populate as you go — add things like: how you prefer to review work, commit message style)
```

Also create empty directories `.claude/memory/tools/` and `.claude/memory/domain/` if they do not exist (these fill up over time as you work).

---

## 2. Update CLAUDE.md

If `.claude/CLAUDE.md` does not exist, create it as an empty file first.

For each of the sections below, check if the section header already exists in the file. If it does, leave it alone. If it does not, add it:
- New or empty file: add sections at the top
- File with existing content: add sections after the last existing section

**Section: Memory Management**

```
## Memory Management

Maintain a structured memory system rooted at .claude/memory/

### Structure

- memory.md — index of all memory files, updated whenever you create or modify one
- general.md — cross-project facts, preferences, environment setup
- domain/{topic}.md — domain-specific knowledge (one file per topic)
- tools/{tool}.md — tool configs, CLI patterns, workarounds

### Rules

1. When you learn something worth remembering, write it to the right file immediately
2. Keep memory.md as a current index with one-line descriptions
3. Entries: date, what, why — nothing more
4. Read memory.md at session start. Load other files only when relevant
5. If a file doesn't exist yet, create it
6. Before removing or modifying any existing memory entry, use AskUserQuestion to confirm
   with the user — show the current content and the proposed change

### Maintenance

When I say "reorganize memory":
1. Read all memory files
2. Remove duplicates and outdated entries
3. Merge entries that belong together
4. Split files that cover too many topics
5. Re-sort entries by date within each file
6. Update memory.md index
7. Show me a summary of what changed
```

**Section: Global Memory**

```
## Global Memory

Read .claude/memory/memory.md at session start. Load specific topic files only when relevant.

Topic files:
- .claude/memory/general.md — cross-project conventions and preferences
```

**Section: Global Memory Reference Rule**

```
## Global Memory Reference Rule

Whenever you work in a project and read (or create) its MEMORY.md, check that it contains a
## Global Memory section. If it does not, add it near the top, after the H1.

The section must be a SHORT POINTER only. Do NOT duplicate the topic file list into project
MEMORY.md. The list lives in CLAUDE.md (single source of truth). Project MEMORY.md has a
200-line budget — use it for project knowledge, not boilerplate.

Canonical template for project MEMORY.md:

## Global Memory

Read .claude/CLAUDE.md for memory rules and topic files.

When a new file is added to .claude/memory/:
- Add it to the ## Global Memory topic file list in .claude/CLAUDE.md only
- Do NOT update individual project MEMORY.md files
```

**Section: Repo Memory Auto-Init**

```
## Repo Memory Auto-Init

At session start in any project, check for MEMORY.md in the project memory directory
(.claude/projects/{mapped-path}/memory/). If it does not exist, create it:

# {Project Name} - Project Memory

## Global Memory

Read .claude/CLAUDE.md for memory rules and topic files.

## Project Notes

(Populated as you work in this project)
```

**Section: Domain Knowledge Lifecycle**

```
## Domain Knowledge Lifecycle

1. Staging — knowledge accumulates in .claude/memory/domain/{name}/
2. Promotion — enough knowledge exists to package as a plugin/skill
3. Pointer — after promotion, the memory file becomes a pointer to the plugin;
   content lives in the plugin

When an update is needed to a promoted domain, note it in the memory file so an issue
can be created on the plugin repo.
```

---

## 3. Initialise project MEMORY.md files

Scan `.claude/projects/` for existing project directories. For each one:

a) If `memory/MEMORY.md` does not exist: create it with the stub template above (use the directory name as the project name, formatted readably).

b) If `memory/MEMORY.md` exists and contains a ## Global Memory section with a trigger table (a markdown table with Trigger/Load columns): replace that section with the 2-line pointer. Do not modify any other content in the file.

c) If `memory/MEMORY.md` exists with a correct pointer already: leave it alone.