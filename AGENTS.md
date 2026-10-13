# ScummVM Agent Guidance

## Contribution constraints

- Keep contributions GPLv3+-compatible. For contribution process, portability, and commit conventions, see [CONTRIBUTING.md](CONTRIBUTING.md).
- Follow the repository's [.editorconfig](.editorconfig) and [.clang-format](.clang-format). Avoid lengthy comments and unrelated changes.

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
- Game-specific implementations live under `engines/`. Before changing an engine, inspect its local `module.mk` and configure metadata as well as nearby implementations and tests.
- Build options and engine registration are generated/configured through the repository build system; avoid assuming a source file is automatically part of every build.

For product context and user-facing documentation, start with [README.md](README.md). Use the project documentation and developer wiki linked there rather than duplicating their content here.
