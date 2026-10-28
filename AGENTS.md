# ScummVM Agent Guidance

## Contribution constraints

- Keep contributions GPLv3+-compatible. For contribution process, portability, and commit conventions, see [CONTRIBUTING.md](CONTRIBUTING.md).
- Follow the repository's [.editorconfig](.editorconfig) and [.clang-format](.clang-format). Avoid lengthy comments and unrelated changes.

## Commit messages

- Follow ScummVM's [commit message guidelines](https://wiki.scummvm.org/index.php/Commit_Guidelines).
- Prefix engine-specific commits with the uppercase engine name and a colon, followed by a concise description (for example, `COMPLET: Add game detection`).

## Portability conventions

From the wiki's [Coding Conventions](https://wiki.scummvm.org/index.php?title=Coding_Conventions) and [HOWTO-Open Files](https://wiki.scummvm.org/index.php?title=HOWTO-Open_Files):

- Every source file starts with the standard ScummVM GPLv3+ license header (copy it from an existing file).
- C++11 subset only: no exceptions, no `std::`/STL in new engines (use `common/` classes), no global objects with constructors or destructors.
- No non-const `static` variables inside functions and no mutable globals; keep state in engine members so "Return to launcher" and restarting a game start clean.
- Never assume endianness or struct layout: read and write multi-byte values with `READ_LE_UINT16`/`WRITE_BE_UINT32` etc. or the stream methods (`readUint32LE()`), and use `common/pack-start.h`/`pack-end.h` if packing is unavoidable.
- No `fopen`/`fread`/`open` or absolute paths. Open game files with `Common::File::open()` and relative, `/`-separated names resolved through `SearchMan`; turn config paths into a `Common::FSNode` first. Write saves and other persistent data only through the `SaveFileManager`.
- Guard optional libraries (`USE_MAD`, `USE_FREETYPE2`, ...) with `#ifdef`, or declare hard requirements in the engine's `configure.engine`.

## Build and tests

- For a standard local build, run `./configure` and then `make -j2`. Configure options and optional dependency requirements vary by platform; consult `./configure --help` and the relevant CI job in [.github/workflows/ci.yml](.github/workflows/ci.yml).
- Run the CxxTest unit suite with `make test`; see [test/README](test/README) for details.
- A full build depends on locally installed development libraries. Report unavailable dependencies or unrun checks rather than implying they passed.

## Repository layout

- Shared runtime code is organized by subsystem in `common/`, `audio/`, `graphics/`, `image/`, `math/`, `video/`, and `gui/`; platform integration lives in `backends/`.
- Game-specific implementations live under `engines/`. Before changing an engine, read its `engines/<engine>/README.md` for existing technical documentation, then inspect its local `module.mk` and configure metadata as well as nearby implementations and tests. Treat the engine README as the home for engine-specific technical documentation and keep it current when work adds or changes relevant knowledge.
- For reverse-engineering work on an engine or its games, document the discoveries, supporting evidence, limitations, and results in `engines/<engine>/README.md`. Use the technical-writer agent to draft or update this documentation from the investigation findings, and verify it accurately reflects the evidence before finishing.
- Build options and engine registration are generated/configured through the repository build system; avoid assuming a source file is automatically part of every build.

For product context and user-facing documentation, start with [README.md](README.md). Use the project documentation and developer wiki linked there rather than duplicating their content here.

## Machine-local development environment

When working on legacy game engines, game compatibility,
or testing with original game assets, read `AGENTS.local.md`
if it exists.

It contains machine-specific paths and environment details.
Do not assume those paths exist on other machines, and do
not copy machine-specific paths into tracked files unless
explicitly required.
