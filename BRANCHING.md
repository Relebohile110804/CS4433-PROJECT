# Branching and release rules

The project uses a simple feature-branch workflow (GitHub Flow) with tagged releases.

## Branches

- `main` always builds and passes every CI check. Work is never committed straight to it.
- Each piece of work gets its own short-lived branch made from the latest `main`:
  - `feature/<what>` for new work, for example `feature/phase6-quality`
  - `fix/<what>` for a defect, for example `fix/password-masking`
  - `docs/<what>` for documentation only
- A branch is merged into `main` through a pull request, and only when both CI jobs (build-and-test, static-analysis) are green. The branch is deleted after the merge.

## Commits

- One commit per finished step, with a message that says what changed and why, and names the requirement, defect (D-xx) or review finding (CR-xx) where there is one.
- Run the full test suite on the laptop before pushing (`lms_tests`), after a clean build.

## Releases and tags

- A release is an annotated tag on `main`, named with semantic versioning `vMAJOR.MINOR.PATCH`:
  - MINOR goes up for new features or a finished project phase, PATCH for fixes only.
  - `v1.0.0` is the first version handed over to the school.
- Planned tags: `v0.9.0` after Phase 6 (quality assurance), `v1.0.0` after Phase 7 (deployment), `v1.1.0` for change request CR1 in Phase 8.
- A tag is only made on a commit whose CI run is green, and it is never moved once pushed.
