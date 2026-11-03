# Building TDLib for BeyGram Windows

BeyGram Windows uses TDLib's official **JSON interface** (`tdjson.dll`). This
document records the exact source revision, dependencies, build commands and
verification used by the Windows client. TDLib is not vendored and is not built
by the Android Gradle project.

## Pinned revision

| Item | Value |
|---|---|
| Repository | <https://github.com/tdlib/td> |
| Commit | `4c6377aa7386c39217878a9a56d581922ad37ca4` |
| Commit date | 2026-10-27 |
| CMake project version | 1.8.69 (reported by the built client) |
| JSON library | `tdjson` (shared) |

Do not track a floating branch for release builds. Update this file and the CI
workflow together when bumping the revision.

## Dependencies

| Dependency | Version used | Notes |
|---|---|---|
| Visual Studio 2022 | 17.14, MSVC 19.44 | x64 toolset |
| CMake | 4.1.2 (bundled with VS) | any 3.24+ works |
| vcpkg | 2026-10-29 | supplies OpenSSL and zlib |
| OpenSSL | 3.6.5 (`x64-windows`) | via vcpkg |
| zlib | 1.3.2 (`x64-windows`) | via vcpkg |
| gperf | 3.3 | required during the build |
| PHP | 8.3 | used by TDLib source generation |

`gperf` and `php` must be on `PATH`. On this machine they come from WinGet.
OpenSSL is **not** taken from a random system PATH: the vcpkg toolchain points
the build at the exact libraries.

## Reproducible build

```powershell
# 1. Dependencies (once)
git clone --depth 1 https://github.com/microsoft/vcpkg.git F:\vcpkg
F:\vcpkg\bootstrap-vcpkg.bat
F:\vcpkg\vcpkg.exe install openssl:x64-windows zlib:x64-windows --triplet x64-windows

# 2. Pinned TDLib source
git clone https://github.com/tdlib/td.git F:\tdlib\td
git -C F:\tdlib\td checkout 4c6377aa7386c39217878a9a56d581922ad37ca4

# 3. Configure and build only the JSON interface
cmake -S F:\tdlib\td -B F:\tdlib\build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=F:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCMAKE_BUILD_TYPE=Release `
  -DTD_ENABLE_LTO=OFF `
  -DTD_ENABLE_PCH=OFF `
  -DTD_ENABLE_MULTI_PROCESSOR_COMPILATION=ON

cmake --build F:\tdlib\build --config Release --target tdjson --parallel 12
```

The DLL and its runtime dependencies are produced under the TDLib build tree
(the exact folder depends on the generator; for the Visual Studio generator it
is `F:\tdlib\build\bin\Release`). Copy or point BeyGram at the folder containing
`tdjson.dll`, `libcrypto-3-x64.dll`, `libssl-3-x64.dll` and `zlib1.dll`.

The MSVC build compiles TDLib's large generated translation units in one batch.
Expect roughly 20–40 minutes and a peak of several gigabytes of RAM on the
first build. `-DTD_ENABLE_MULTI_PROCESSOR_COMPILATION=ON` can speed this up on a
machine with enough memory; CI keeps it off to avoid exhausting the runner.
Subsequent builds are incremental.

## Configure BeyGram with TDLib

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DBEYGRAM_TDLIB_DIR=F:\tdlib\build\bin\Release `
  -DBEYGRAM_BUILD_TESTS=ON

cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`cmake/BeyGramTdlib.cmake` finds `tdjson.dll`, records its directory and copies
`tdjson.dll` plus every DLL beside it to the BeyGram executable directory after
each build. At runtime the application loads only:

1. `<application dir>\tdjson.dll`,
2. `<application dir>\tdlib\tdjson.dll`,
3. an **absolute** path from `BEYGRAM_TDLIB_DLL`.

`PATH` is deliberately not part of the search. A relative `BEYGRAM_TDLIB_DLL`
is rejected, and an override outside the application directory is reported as
such in the Environment page. This narrows the DLL-hijacking surface while
keeping a deliberate developer override.

## Verify the exports

`tdjson.dll` must export the five functions used by `TdJsonTransport`:

```powershell
dumpbin /exports F:\tdlib\build\bin\Release\tdjson.dll |
  Select-String "td_json_client_"
```

Expected symbols:

- `td_json_client_create`
- `td_json_client_send`
- `td_json_client_receive`
- `td_json_client_execute`
- `td_json_client_destroy`

`TdJsonTransport::load()` refuses a DLL that loads but lacks any of them.

## Integration tests

`tst_tdlib_integration` uses the real DLL but **not a real Telegram account**:

1. loads the trusted DLL and executes a local TDLib method;
2. creates a temporary TDLib database with throwaway API credentials;
3. waits for `authorizationStateWaitPhoneNumber`;
4. closes the session and reopens the same database to verify restart and lock
   release.

It never submits a phone number, code, password or QR confirmation. Wrong-code
and wrong-2FA paths require a dedicated test account and are not run
automatically; see `docs/PORTING_ROADMAP.md`.

Run it directly:

```powershell
$env:BEYGRAM_TDLIB_DLL = "F:\tdlib\build\bin\Release\tdjson.dll"
.\build\Release\tst_tdlib_integration.exe -o -,txt
```

Without a DLL the test skips with an explicit message instead of failing the
whole suite.

## Runtime dependencies and licensing

- TDLib is distributed under the Boost Software License; see the TDLib checkout.
- OpenSSL is Apache-2.0; zlib is zlib-licensed.
- BeyGram does not modify TDLib sources. Any patch must be recorded here and in
  `THIRD_PARTY_NOTICES.md`.
