# syntax=docker/dockerfile:1
ARG TARGETOS
ARG TARGETARCH
ARG RUNTIME_LINUX=mcr.microsoft.com/dotnet/runtime-deps:10.0
ARG BASE_WINDOWS=mcr.microsoft.com/windows/nanoserver:ltsc2022

FROM --platform=$BUILDPLATFORM mcr.microsoft.com/dotnet/sdk:10.0 AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends clang zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY app/app.csproj ./
RUN dotnet restore -r linux-x64
COPY app/*.cs ./
RUN dotnet publish -c Release -r linux-x64 --no-restore -o /out

FROM ${RUNTIME_LINUX} AS final-linux-amd64
WORKDIR /app
COPY --from=build /out/app .
ENTRYPOINT ["./app"]

FROM ${RUNTIME_LINUX} AS final-linux-arm64
WORKDIR /app
COPY dist/linux-arm64/app .
ENTRYPOINT ["./app"]

FROM ${BASE_WINDOWS} AS final-windows-amd64
WORKDIR /app
COPY dist/win-x64/app.exe .
ENTRYPOINT ["app.exe"]

FROM final-${TARGETOS}-${TARGETARCH}
ENV ASPNETCORE_HTTP_PORTS=8080
EXPOSE 8080
