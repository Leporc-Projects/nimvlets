# Nimvlets — Local State Persistence (Block 03)

This describes the small, local-only persistence layer built in Block 03
on top of Block 02's content/animation runtime. It is not a UI block —
nothing here adds a Shop, Collection, onboarding, or pet-selection
screen. See `docs/ANIMATION_RUNTIME.md` for the runtime this plugs
into, and `docs/DECISION_LOG.md` DEC-025 through DEC-028 for why each
choice below was made.

## 1. Scope

Persisted, with real runtime meaning today:

- **Click balance** (`AppState::clickBalance`, `uint64`) — the only
  currency (AGENTS.md §2). Incremented on every valid click; never
  decremented in this block (no Shop exists yet to spend it).
- **Active pet id** (`AppState::activePetId`) — kept truthfully in
  sync with whichever pet actually loaded (`content::PetDefinition::id`).
  This block implements no pet *selection*: exactly one pack always
  loads, so the field always reflects that one pet. It exists so a
  future selection UI has somewhere to read/write from without a
  schema change.
- **Active variant id** (`AppState::activeVariantId`) — carried
  through load/save; nothing in this block writes a non-empty value
  (no variant selection exists yet — see
  `content::PetDefinition::variantGroup`, also schema-only).
- **Last window position** (`AppState::lastWindowPosition`) — updated
  whenever a drag ends; used to reopen the window where the user left
  it (see §7).

Not persisted, and not planned for this block (see §9): purchase
history, unlock state, onboarding/starter-selection state, anything
Shop/Collection-shaped, provenance/analytics of any kind.

Generic by construction: `activePetId`/`activeVariantId` are plain
strings, not an enum of known Nimvlets — adding a new pet id or variant
later never requires a change to `src/persistence`.

## 2. Storage location policy

Production: `SDL_GetPrefPath("Leporc Projects", "Nimvlets")` — SDL's
own cross-platform per-user app-data resolver. It creates the
directory itself if needed and returns:

- macOS: `~/Library/Application Support/Leporc Projects/Nimvlets/`
- Windows: `%APPDATA%\Leporc Projects\Nimvlets\`

No platform-specific code was needed for this (unlike window
transparency/click-through) — SDL already fully abstracts it, so
`src/platform/*` is untouched by this block.

**DEV-only override:** `NIMVLETS_DEV_APPDATA_DIR`, checked before
`SDL_GetPrefPath()` is ever called. If set to a non-empty path, that
path is used instead (created if it doesn't exist) — production
behavior (the unset case) is unchanged. This is what lets manual QA
and this block's own non-interactive smoke tests exercise the real
save/load path against an isolated temp directory, never the real
per-user location — mirrors `NIMVLETS_DEV_PASSIVE_INTERVAL_SECONDS`'s
pattern from Block 02 exactly (see `docs/ANIMATION_RUNTIME.md` §8).

```bash
NIMVLETS_DEV_APPDATA_DIR=/tmp/nimvlets_dev_state ./build/macos-debug/src/app/nimvlets_spike
```

One file inside that directory: `state.nvstate` (plus a transient
`state.nvstate.tmp` staging file that only exists mid-write — see §4).

No absolute, machine-specific path is ever hardcoded or committed —
`SDL_GetPrefPath()`'s result is a runtime value, and the DEV override
is an environment variable, never a literal path in source.

## 3. File format ("NVSTATE1")

Producer/consumer: `persistence::SerializeAppState` /
`DeserializeAppState` (`src/persistence/AppStateSerializer.cpp`). Pure
— no file I/O — so it's directly unit-testable with in-memory byte
buffers (`tests/AppStateSerializerTest.cpp`), the same separation
`content::PetPackLoader` established in Block 02. All integers
little-endian (every platform this project targets — x86_64, arm64 —
is little-endian; no byte-swapping is implemented, matching every
other on-disk format in this repository).

```
magic             : 8 bytes, "NVSTATE1"
schemaVersion     : uint32
clickBalance      : uint64
activePetId       : string   (uint32 byte-length + UTF-8 bytes)
activeVariantId   : string
hasWindowPosition : uint8   (0/1)
lastWindowX       : int32   (meaningful only if hasWindowPosition)
lastWindowY       : int32
```

**Deterministic:** serializing the same `AppState` twice produces
byte-identical output — no timestamps, no padding, no map/set
iteration order anywhere in the format
(`tests/AppStateSerializerTest.cpp`'s `SerializationIsDeterministic`).

**Versioned, no migration in this block:** `DeserializeAppState`
rejects any `schemaVersion` that doesn't exactly equal
`AppState::kCurrentSchemaVersion` (currently 1) — an older *or* newer
version is treated identically to corrupt data (see §5), never guessed
at. There is exactly one supported version right now; a future block
adding migration logic has a clean, obvious seam (`schemaVersion` is
already read and checked first, before anything else) rather than
needing to retrofit one.

## 4. Atomic write behavior

`persistence::AppStateStore::Save()` (`src/persistence/AppStateStore.cpp`):

1. Serialize the full `AppState`.
2. Write it to `state.nvstate.tmp` in the same directory. If this
   fails at any point (can't open, can't write the full contents), the
   real `state.nvstate` is never touched — `Save()` returns `false`
   with a specific `outError` and stops here.
3. Only if step 2 fully succeeded: `std::filesystem::rename(tempPath,
   statePath)`. Same-directory rename is atomic on the filesystems
   this project targets — the real file either has the complete old
   contents or the complete new contents, never a partial write, even
   if the process crashes, loses power, or the disk fills up mid-write.

This is the same reason step 2's failure leaves the *previous* valid
save completely untouched: the real file is never opened for writing
directly, only ever replaced in one atomic step at the very end.
`tests/AppStateStoreTest.cpp`'s `FailedWritePreservesPriorValidSave`
demonstrates this directly (see §8 for how a write failure is
simulated portably in tests).

A failed `Save()` is reported (`outError`, logged by `SpikeApp` via
`SDL_Log`) and never crashes the app — see §6 for what happens to the
pending change afterward.

## 5. Corruption recovery

`AppStateStore::Load()` never throws and always returns a usable
`AppState`:

| Situation | Result |
|---|---|
| No `state.nvstate` file yet (first run) | `AppState{}` (safe defaults) |
| File exists but can't be opened/read | `AppState{}` |
| File parses but bad magic / truncated / unsupported schema version | `AppState{}` |
| File parses and schema version matches | the parsed state |

Every "safe defaults" case sets an optional `outWarning` string (e.g.
*"existing app-state save could not be used (...); using defaults"*)
so the caller can log *why*, without forcing every call site to handle
a separate error path — a corrupt or unreadable save is never
silently indistinguishable from "no save yet" in the log, even though
both produce the same safe in-memory result.
`tests/AppStateStoreTest.cpp`'s `LoadRecoversFromCorruptFile` writes
garbage bytes directly (bypassing `Save()`) to simulate real on-disk
corruption, not just a synthetic parser input.

## 6. Debounce / write policy

Clicks can arrive many times per second; writing to disk once per
click would be wasteful and pointless. `persistence::PersistenceScheduler`
(`src/persistence/PersistenceScheduler.h`, pure, no file I/O, testable
with fabricated timestamps exactly like `core::FrameScheduler`):

- The **first** change after a clean/flushed state arms a deadline
  **2000ms** (`PersistenceScheduler::kDefaultDebounceMs`) in the
  future — short enough that a crash shortly after the last change
  loses at most ~2 seconds of progress, long enough that a realistic
  burst of rapid clicks collapses into one write.
- Any further change **before** that deadline fires is a no-op for
  scheduling purposes: it updates the in-memory `AppState` but does
  **not** arm a new deadline or push the existing one out. This is
  what makes "10 rapid clicks" cost exactly one disk write, not ten,
  and also what stops continuous activity from starving persistence
  indefinitely (a sliding/reset-on-activity debounce could, in
  principle, never fire under nonstop clicking; a fixed window from
  the *first* pending change can't).
- A **failed** flush leaves the state dirty (the pending change is not
  silently dropped) but reschedules the retry a further `debounceMs`
  out, rather than retrying on literally the next event-loop wake —
  bounding retry frequency to at most once per debounce interval even
  under a persistent failure (e.g. a deleted directory). No exponential
  backoff, no retry limit — not needed at this scale, and not asked
  for.
- **Clean shutdown always flushes** whatever's still dirty,
  unconditionally, regardless of the debounce deadline
  (`SpikeApp::Shutdown()` calls `FlushPersistedState()` before tearing
  down anything else).

`PersistenceScheduler::NextFlushDeadlineMs()` folds into
`SpikeApp::Run()`'s existing `SDL_WaitEventTimeout` deadline
calculation exactly the way the animation/passive-action deadlines
already do (see `docs/ANIMATION_RUNTIME.md` §6) — no new polling loop,
no timer thread; the event loop simply also wakes up when a pending
flush is due.

## 7. Runtime integration

| Event | Effect |
|---|---|
| Startup | Resolve the app-data directory (§2); load existing state or defaults; sync `activePetId` to the pack that actually loaded (marks dirty if it changed); if a window position was saved, open there instead of centered. |
| Click | `clickCount_` (session-only diagnostic) and `appState_.clickBalance` (persisted) both increment; scheduler marked dirty. Deliberately two separate counters — see `docs/DECISION_LOG.md` DEC-026. |
| Drag end | `appState_.lastWindowPosition` set to the window's final position; scheduler marked dirty. |
| Event-loop wake, flush deadline reached | `FlushPersistedState()` — a no-op unless actually dirty. |
| Clean shutdown | `FlushPersistedState()` unconditionally (ignores the deadline; still a no-op if nothing is dirty). |

**No off-screen/monitor-bounds validation.** A saved window position is
restored exactly as stored, even if the display configuration changed
since (a monitor was unplugged, resolution changed). Not attempted in
this block — see the Block 03 report's limitations.

## 8. Testability

`src/persistence` has no SDL dependency at all, so every piece is
directly unit-testable without a display:

- `tests/AppStateSerializerTest.cpp` — pure round-trip/determinism/
  corruption tests against in-memory byte buffers (10 cases).
- `tests/AppStateStoreTest.cpp` — real, isolated temp directories
  (created fresh per test, removed after — never the real per-user
  location), covering defaults, round-trip, atomicity (no leftover
  `.tmp` file), corruption recovery, and write-failure handling.
  Write failures are simulated by pre-creating a *directory* at the
  exact path `Save()` would use for its temp file — opening a
  directory for writing as a regular file fails uniformly on every
  platform this project targets, avoiding fragile, platform-dependent
  `chmod`-based permission tricks (POSIX and Windows disagree enough
  on those to make CI flaky).
- `tests/PersistenceSchedulerTest.cpp` — debounce/coalescing/retry
  behavior with fabricated timestamps (8 cases).
- `tests/PersistenceIntegrationTest.cpp` — the same click/drag/flush/
  shutdown sequence `SpikeApp` actually performs, wired together with
  real (temp-directory) `AppStateStore` + `PersistenceScheduler`, the
  same pattern `tests/ClickAccountingTest.cpp` established in Block 02
  for click/drag classification.

All four run through the same `ctest` invocation as every other test
in this repository; none require a display or the real app-data
directory.

## 9. A shutdown-responsiveness bug found and fixed by this block's own testing

While building this block's required non-interactive smoke tests
(§6's "no manual QA" constraint), a real, pre-existing latency issue
surfaced: `SDL_WaitEventTimeout` does not itself get interrupted by a
delivered `SIGINT`/`SIGTERM` on this platform — the event loop only
re-checks `ShutdownRequested()` when its *own* wait actually returns
(a real event, or the requested timeout elapsing). Once a truly static
idle stretch has nothing else scheduled for minutes (the ~300s
passive-action deadline being the only one left), a termination signal
could take up to that long to be noticed — technically present since
Block 01/02, just never exercised by a test that let the app fully
settle before signaling it.

Fixed by capping the event loop's maximum wait to 1000ms
(`src/app/SpikeApp.cpp`'s `kMaxWaitMs`), regardless of how far away the
real next deadline is. This bounds shutdown latency to about a second
without reintroducing a render tick: a wake that finds nothing to do
(`ShutdownRequested()` false, no deadline actually reached) does zero
redraw/hit-mask/disk work before going back to sleep — confirmed by
re-measuring static-idle CPU after the fix (still ≈0.0%, see
`docs/PERFORMANCE_BUDGETS.md`). See `docs/DECISION_LOG.md` DEC-028.

## 10. Intentionally unimplemented

- **No pet-selection UI or logic.** `activePetId`/`activeVariantId`
  persist and round-trip, but nothing branches on them to choose which
  pack loads.
- **No schema migration.** Exactly one supported `schemaVersion`;
  anything else is treated as unusable, not upgraded.
- **No unlock/purchase/economy state.** No Shop exists yet to spend
  `clickBalance` against.
- **No window-position bounds/monitor validation** (see §7).
- **No encryption, no cloud sync, no account.** Purely local,
  unauthenticated, single-file storage — see AGENTS.md §5 and
  `docs/PRIVACY_SECURITY.md`.
