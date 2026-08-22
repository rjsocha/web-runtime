# dotnet-aot

The dotnet app published with NativeAOT: no runtime in the image, no portable
payload, a native binary per platform.

```sh
make publish      # builds the Windows binary on WINHOST over ssh
make image        # wyga/web-runtime-dotnet-aot:2019, :2022, :2025, :latest
make run
```

NativeAOT does not cross-compile between operating systems: the Linux binary
needs clang in a Linux SDK image, and the Windows one needs the MSVC toolchain,
so it can only be produced on Windows. `make publish` copies the sources to
`WINHOST` (default `wx`), runs `dotnet publish -r win-x64` there and brings the
exe back into `dist/`. From then on the Windows stage is only a COPY, so a
single build covers all three platforms and pushes one index per Windows base.

The Linux image sits on `runtime-deps` rather than scratch - the AOT binary is
native but not static, and still wants glibc. On Windows it needs nothing
beyond Nano Server: the exe imports only the `api-ms-win-crt-*` sets, with no
`vcruntime140`.
