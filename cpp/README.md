# cpp

Runtime-info page over HTTP on Boost.Beast, which is header-only, so nothing of
Boost is built - only the compiler matters.

```sh
make check        # compile all three targets, push nothing
make image        # wyga/web-runtime-cpp:2022, :2025, :latest
make run
```

One Dockerfile cross-compiles every target from the build host: `g++` for
linux/amd64, `aarch64-linux-gnu-g++` for linux/arm64 and
`x86_64-w64-mingw32ucrt-g++` for windows/amd64. Everything is linked statically,
so the Linux entries sit on scratch and the Windows one only needs nanoserver.

The Windows toolchain is the UCRT one rather than the default mingw: the
default links `msvcrt.dll`, the UCRT build links the `api-ms-win-crt-*` sets
that Nano Server carries.
