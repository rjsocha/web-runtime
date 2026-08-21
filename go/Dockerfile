# syntax=docker/dockerfile:1
ARG TARGETOS
ARG GO=golang:1.27
ARG BASE_WINDOWS=mcr.microsoft.com/windows/nanoserver:ltsc2022

FROM --platform=$BUILDPLATFORM ${GO} AS build
WORKDIR /src
COPY go.mod ./
RUN go mod download
COPY *.go ./
ARG TARGETOS TARGETARCH
RUN CGO_ENABLED=0 GOOS=$TARGETOS GOARCH=$TARGETARCH go build -trimpath -ldflags "-s -w" -o /out/server .

FROM scratch AS final-linux
COPY --from=build /out/server /server
ENTRYPOINT ["/server"]

FROM --platform=windows/amd64 ${BASE_WINDOWS} AS final-windows
COPY --from=build /out/server /server.exe
ENTRYPOINT ["\\server.exe"]

FROM final-${TARGETOS}
ENV PORT=8080
EXPOSE 8080
