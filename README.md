# For demo purposes only ...

Web runtime-info apps as multi-platform container images.

| project | language |
|---|---|
| [dotnet](dotnet/) | C# / ASP.NET Core on .NET 10 |
| [go](go/) | Go, net/http, static binary |
| [java](java/) | Java 25 LTS, com.sun.net.httpserver, virtual threads |
| [java-javalin](java-javalin/) | Java 25 LTS, Javalin on Jetty, Maven |
| [cpp](cpp/) | C++20, Boost.Beast, static binary |
| [rust](rust/) | Rust, hyper on tokio, static binary |

## Run

Every app listens on 8080 inside the container.

```sh
docker run --rm -p 8080:8080 wyga/web-runtime-go:latest
curl localhost:8080
curl localhost:8080/verbose
curl localhost:8080/env
```

## Build

Each project builds with `make` and pushes to the registry. `IMAGE` names the
target, so another registry needs no edit:

```sh
make -C go image IMAGE=registry.example.com/team/web-runtime-go
make -C dotnet all IMAGE=registry.example.com/team/web-runtime-dotnet
```

In dotnet the payload follows `IMAGE` as `$(IMAGE)-payload:latest` and reaches
the runtime build as a build argument; `PAYLOAD` overrides it on its own.

## Images

## Deployable

| image | platforms |
|---|---|
| `wyga/web-runtime-dotnet:latest` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-dotnet:2025` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-dotnet:2022` | linux/amd64, linux/arm64, windows/amd64 (ltsc2022) |
| `wyga/web-runtime-go:latest` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-go:2025` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-go:2022` | linux/amd64, linux/arm64, windows/amd64 (ltsc2022) |
| `wyga/web-runtime-go:2019` | linux/amd64, linux/arm64, windows/amd64 (ltsc2019) |
| `wyga/web-runtime-java:latest` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-java:2025` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-java:2022` | linux/amd64, linux/arm64, windows/amd64 (ltsc2022) |
| `wyga/web-runtime-java-javalin:latest` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-java-javalin:2025` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-java-javalin:2022` | linux/amd64, linux/arm64, windows/amd64 (ltsc2022) |
| `wyga/web-runtime-cpp:latest` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-cpp:2025` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-cpp:2022` | linux/amd64, linux/arm64, windows/amd64 (ltsc2022) |
| `wyga/web-runtime-rust:latest` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-rust:2025` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-rust:2022` | linux/amd64, linux/arm64, windows/amd64 (ltsc2022) |

## Payload

| image | description |
|---|---|
| `wyga/web-runtime-dotnet-payload:latest` |  published IL, not runnable |
| `wyga/web-runtime-java-payload:latest` |  app.jar, not runnable |
| `wyga/web-runtime-java-javalin-payload:latest` |  shaded app.jar, not runnable |

