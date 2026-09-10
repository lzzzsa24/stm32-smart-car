# Canonical branch and worktree workflow

## One authoritative project

After the four-mode consolidation (rc.6), modes1/2 use the old main controllers,
mode3 uses comprehensive a217df8 mode3, and mode4 uses its mode5. Number5 is
unassigned. The old comprehensive-v15 branch is a retained source snapshot,
not the base for new work. Start future fixes from the new main and name the
new mode number in handoffs; do not blindly merge a worker's old main.c selector.

- Repository and canonical checkout:
  `F:/myproject/jidian/project/test-exp7-unified-motion-v1`
- GitHub: `https://github.com/lzzzsa24/stm32-smart-car`
- Canonical integration/deployment branch: `main`
- Shared live status: `PROJECT_STATE.md`
- Formal build entrypoint: `build_unified_motion.ps1`

Only `main` answers “what is the latest integrated car project?”. Existing
worktrees remain useful history, but they are not alternative main projects.

## Branch classes

| Name | Purpose | May update shared state / flash / push? |
|---|---|---|
| `main` | reviewed, integrated, buildable firmware | coordinator only, with current user authorization for flash/push |
| `feature/<topic>` | one bounded new feature | no; return commits to the coordinator |
| `fix/<topic>` | one bounded correction | no; return commits to the coordinator |
| `test/<topic>` | isolated diagnostic or comparison | no; never becomes latest merely because it was tested |
| `rollback/...` tag | immutable recovery point | never move or delete casually |
| `deployed/...` tag | exact source of a verified deployment | points to source, while `PROJECT_STATE.md` records evidence |

The older worktrees under `F:/myproject/jidian/worktrees` are retained rather
than deleted. Their branch tips are historical inputs. Start all future work
from current `main`, not from one of those tips.

## Preserved legacy branch inventory

| Branch group | Status after canonicalization | Use |
|---|---|---|
| `fix/mode34-recognition-slowdown` | integrated through `fc5a7b9` | deployment-history reference; do not add new work |
| `feature/mode5-visual-line-v4` | original worker source ending at `5d761e4` | provenance/comparison for mode 5; integrated equivalent is on `main` |
| `feature/line-reacquire-lock` | older integrated baseline | historical source only |
| `feature/line-search-axle-balance` | earlier line-search worker | historical source only |
| `feature/mode3-sign-line` | earlier mode 3/4 worker | historical source only |
| `feature/k210-visual-line` | earlier VL1 experiment | historical source only |
| `feature/simple-four-line` | standalone SL2 experiment | historical source only |
| `test/*` | isolated comparison images | never use as the integrated base |

This inventory is organizational, not authorization to delete anything. Once
the user confirms no old tasks still need these worktrees, pruning can be done
as a separate, explicitly approved cleanup.

## Starting one feature

From the canonical checkout, first confirm `main` is clean and current. Then
create exactly one branch/worktree for one bounded task:

```powershell
git status --short --branch
git fetch origin
git worktree add F:/myproject/jidian/worktrees/<topic> -b feature/<topic> main
```

The worker reads `AGENTS.md` and `PROJECT_STATE.md`, changes only its assigned
files, runs focused tests, commits, and returns its commit SHA. It does not
flash, push, merge, or edit `PROJECT_STATE.md`.

## Integrating a finished feature

All integration happens in the canonical checkout on `main`:

```powershell
git switch main
git status --short --branch
git cherry-pick -x <worker-commit>
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_unified_motion.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\check_project_state.ps1
```

Use a normal merge instead of cherry-pick only when a complete multi-commit
branch history is intentionally required. Resolve conflicts once on `main`;
never copy the same fix by hand into several experiment projects.

After tests pass, flashing still requires a fresh explicit user request.
Enumerate ports immediately before deployment, build from the selected source,
preserve the calibration page, verify readback, and update `PROJECT_STATE.md`.
Push `main` and applicable deployment/rollback tags only when authorized.

## Which firmware gets burned

The current task must name the exact source commit and rebuild it. The ignored
`manual-build-*` directories are caches, not version control and not proof of
which source they came from. Ordinary integrated deployment uses `main`.

An explicitly requested historical/feature comparison may be flashed without
merging, but it must be labelled as a temporary test image and must not change
the meaning of “latest”. The coordinator records compile, flash/readback,
lifted-wheel and ground evidence separately.

## GitHub synchronization

`origin/main` is the durable handoff for other computers and new Codex tasks.
Feature branches need to be pushed only when another task/computer must consume
their original commit identity. Old remote branches are preserved until the
user explicitly approves pruning; branch deletion is not part of routine sync.

If GitHub branch protection requires a pull request, push the local `main` tip
to a short-lived `integration/<topic>` branch, open a PR, and record the pending
review in `PROJECT_STATE.md`. Do not bypass required review just to make the
remote branch name catch up.
