# For demo purposes only ...

Web runtime-info apps as multi-platform container images.

| project | language |
|---|---|
| [dotnet](dotnet/) | C# / ASP.NET Core on .NET 10 |
| [go](go/) | Go, net/http, static binary |

## Run

Every app listens on 8080 inside the container.

```sh
docker run --rm -p 8080:8080 wyga/web-runtime-go:latest
curl localhost:8080
curl localhost:8080/verbose
curl localhost:8080/env
```

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

## Payload

| image | description |
|---|---|
| `wyga/web-runtime-dotnet-payload:latest` |  published IL, not runnable |

