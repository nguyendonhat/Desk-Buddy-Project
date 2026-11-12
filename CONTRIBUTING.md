# Contributing to Desk Buddy

You do not need to know the whole codebase to take a small task. Use a GitHub Issue as the task brief and let your AI inspect the repository instructions.

## Workflow for every teammate

1. Update your local copy from `main`.
2. Pick one open GitHub Issue and create a branch named `issue-<number>-short-name`.
3. Ask your AI to read `AGENTS.md`, `CLAUDE.md` (if using Claude), and `STATUS.md`, then implement only that issue.
4. Ask the AI to run the checks listed in `AGENTS.md` and clearly separate successful builds from tests and hardware checks.
5. Review the changed files and the AI's summary. Commit and push the branch, then open a Pull Request linked to the Issue.
6. A teammate reviews the Pull Request; merge only after the required checks and review.
7. Update `STATUS.md` in the same Pull Request if verified project status changed.

## Keep changes safe

- Never commit local config or credentials (`config.h`, `.env*` with real values, `.dev.vars`, OAuth secrets, Wi-Fi passwords, device tokens).
- Do not work directly on `main`.
- Avoid multiple people editing the same file on separate branches at the same time; coordinate ownership in the Issue.
- Wokwi validates only the mock simulation. Real display, wiring, Google access, and Wi-Fi must be labeled unverified until physically tested.
- A maintainer merges reviewed Pull Requests; AI agents should not merge or deploy unless explicitly assigned and authorized.

## Suggested task prompt

> Read `AGENTS.md`, `STATUS.md`, and the project docs. Work only on Issue #<number> in my branch. First summarize the intended change and files you expect to touch. Implement it, run the relevant checks, inspect the diff, and report pass/fail plus anything not verified. Do not commit secrets or claim physical testing unless the hardware was actually tested.
