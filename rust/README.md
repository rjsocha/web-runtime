# rust

Runtime-info page over HTTP on hyper and tokio.

```sh
make check        # compile all three targets, push nothing
make image        # wyga/web-runtime-rust:2022, :2025, :latest
make run
```

One toolchain covers every target: `cargo-zigbuild` uses zig as the linker, so
`x86_64-unknown-linux-musl`, `aarch64-unknown-linux-musl` and
`x86_64-pc-windows-gnu` all build from the same image without assembling three
cross toolchains by hand. The Linux binaries are static and sit on scratch.

The Windows target links `msvcrt.dll` rather than the UCRT: the standard
library ships prebuilt from rustup and is bound to that CRT. Nano Server
carries it, so the image runs as it is.
