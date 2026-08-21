# go

Runtime-info page over HTTP, one image per Windows LTSC base. The binary is
static and needs no runtime, so the Linux entries sit on `scratch` and only the
Windows entry carries a base layer.

```sh
make image        # wyga/web-runtime-go:2019, :2022, :2025, :latest
make run
```

Each tag is an index over `linux/amd64`, `linux/arm64` and `windows/amd64`.
The binary is cross-compiled on the build host, so no emulation is involved.
