# CLAUDE.md

`AGENTS.md` is authoritative for every engineering contract in this
repository — architecture, privacy rules, build commands, C++ style,
Git workflow, everything. Read that file first. This file only adds
notes specific to Claude Code sessions; it does not repeat AGENTS.md.

## Claude Code-specific notes

- Don't run `git push`, `git merge` into `main`, or any publish/release
  command in this repository unless a prompt explicitly instructs
  otherwise for that session — see AGENTS.md §15. Land work on the
  block branch with a clean working tree and stop there.
- SDL3 is fetched via CMake `FetchContent` on first configure — the
  first `cmake --preset ...` in a session will download source and take
  noticeably longer than subsequent ones. This is expected, not a
  hang.
- `build/` (all preset subdirectories) and SDL3's `_deps` cache are
  gitignored; don't try to add them.
- No symlink is used between this file and `AGENTS.md` on purpose
  (symlinks are a recurring source of cross-platform Git friction on
  Windows checkouts) — keep this file's content in sync by hand if
  AGENTS.md's structure changes enough to affect the pointers above.
