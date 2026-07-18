# Contributing to ArZoom

Thank you for helping make screen tutorials and live demonstrations easier to watch.

## Project principles

Contributions should protect ArZoom's core qualities:

- simple for first-time OBS users
- smooth and comfortable motion
- predictable behavior near screen edges
- low overhead during recording and livestreaming
- fail-safe output instead of black frames or crashes
- focused scope rather than feature accumulation

## Good first contributions

- documentation corrections
- Indonesian or additional translations
- reproducible compatibility reports
- Windows installer improvements
- tests for motion and edge invariants
- focused bug fixes
- accessibility improvements

## Before opening a pull request

1. Search existing issues and pull requests.
2. Open an issue first for large behavior or architecture changes.
3. Keep the change focused on one problem.
4. Do not commit build directories, dependency caches, installers, archives, patches, or generated reports.
5. Test the affected workflow and describe the result honestly.

## Local Windows build

Requirements:

- Git
- CMake 3.28+
- Visual Studio 2022 or 2026
- Desktop development with C++
- Inno Setup 6 for installer generation

Run:

```text
build-local-windows.bat
```

The first build prepares OBS dependencies. Later builds reuse `.build/`.

To package an existing successful build:

```text
package-existing-build.bat
```

## Motion test

```bash
g++ -std=c++17 -O2 tests/arzoom-math-test.cpp -o arzoom-math-test
./arzoom-math-test
```

## Coding expectations

- C++17 unless the project version explicitly changes it
- no per-frame heap allocation in the rendering path
- no CPU frame readback
- no file I/O or OBS settings writes inside video tick/render loops
- animation must use elapsed time rather than fixed frame assumptions
- new viewport logic must preserve the edge invariant
- runtime failures must fall back to the original source
- user-visible strings belong in locale files

## Pull request checklist

- [ ] The change solves one clearly described problem.
- [ ] Relevant build or test steps pass.
- [ ] The filter remains safe when resources or mapping are unavailable.
- [ ] Beginner-facing behavior and documentation are updated.
- [ ] No generated build or packaging output is committed.
- [ ] The PR explains user impact and validation limits.

## Bug reports and security

Use the issue templates for normal bugs. Do not publish suspected security vulnerabilities in a public issue; follow [SECURITY.md](SECURITY.md).

By contributing, you agree that your contribution is licensed under GPL-2.0-or-later, consistent with this repository.
