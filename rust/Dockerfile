# syntax=docker/dockerfile:1
ARG TARGETOS
ARG BASE_WINDOWS=mcr.microsoft.com/windows/nanoserver:ltsc2022

FROM --platform=$BUILDPLATFORM rust:slim AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends gcc-mingw-w64-x86-64 gcc-aarch64-linux-gnu \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY Cargo.toml ./
COPY src ./src
ARG TARGETOS TARGETARCH
RUN case "$TARGETOS/$TARGETARCH" in \
      linux/amd64)   TARGET=x86_64-unknown-linux-musl;  BINARY=app     ;; \
      linux/arm64)   TARGET=aarch64-unknown-linux-musl; BINARY=app; \
        export CARGO_TARGET_AARCH64_UNKNOWN_LINUX_MUSL_LINKER=aarch64-linux-gnu-gcc ;; \
      windows/amd64) TARGET=x86_64-pc-windows-gnu;      BINARY=app.exe ;; \
      *) echo "unsupported target $TARGETOS/$TARGETARCH" >&2; exit 1 ;; \
    esac \
 && rustup target add $TARGET \
 && cargo build --release --target $TARGET \
 && mkdir -p /out && cp target/$TARGET/release/$BINARY /out/$BINARY

FROM scratch AS final-linux
COPY --from=build /out/app /app
ENTRYPOINT ["/app"]

FROM ${BASE_WINDOWS} AS final-windows
WORKDIR /app
COPY --from=build /out/app.exe .
ENTRYPOINT ["app.exe"]

FROM final-${TARGETOS}
ENV PORT=8080
EXPOSE 8080
