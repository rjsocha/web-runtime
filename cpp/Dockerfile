# syntax=docker/dockerfile:1
ARG TARGETOS
ARG BASE_WINDOWS=mcr.microsoft.com/windows/nanoserver:ltsc2022

FROM --platform=$BUILDPLATFORM debian:forky-slim AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      g++ g++-aarch64-linux-gnu g++-mingw-w64-ucrt64 binutils-mingw-w64-ucrt64 libboost-dev \
 && rm -rf /var/lib/apt/lists/* \
 && mkdir -p /boost && cp -r /usr/include/boost /boost/
WORKDIR /src
COPY main.cpp ./
ARG TARGETOS TARGETARCH
RUN mkdir -p /out && case "$TARGETOS/$TARGETARCH" in \
      linux/amd64)   CXX=g++;                        OUTPUT=/out/app;     FLAGS="-pthread" ;; \
      linux/arm64)   CXX=aarch64-linux-gnu-g++;      OUTPUT=/out/app;     FLAGS="-pthread" ;; \
      windows/amd64) CXX=x86_64-w64-mingw32ucrt-g++; OUTPUT=/out/app.exe; FLAGS="-D_WIN32_WINNT=0x0A00 -lws2_32 -lmswsock -liphlpapi -lpsapi -lbcrypt" ;; \
      *) echo "unsupported target $TARGETOS/$TARGETARCH" >&2; exit 1 ;; \
    esac \
 && $CXX -std=c++20 -O2 -DNDEBUG -isystem /boost -static -static-libgcc -static-libstdc++ \
      -o $OUTPUT main.cpp $FLAGS

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
