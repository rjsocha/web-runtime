# web-runtime

Web runtime-info apps as multi-platform container images (Linux + Windows).

The same small app in several languages: it answers `text/plain` with what the
container it runs in is made of - runtime, host, memory, interfaces and the
request itself. Built to be deployed on a mixed-OS Docker Swarm and looked at.

| project | language |
|---|---|
| [dotnet](dotnet/) | C# / ASP.NET Core on .NET 10 |
| [go](go/) | Go, net/http, static binary |

## Images

| image | platforms |
|---|---|
| `wyga/web-runtime-dotnet:latest` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-dotnet:2025` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-dotnet:2022` | linux/amd64, linux/arm64, windows/amd64 (ltsc2022) |
| `wyga/web-runtime-dotnet-payload:latest` | linux/amd64, linux/arm64, windows/amd64 - published IL, not runnable |
| `wyga/web-runtime-go:latest` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-go:2025` | linux/amd64, linux/arm64, windows/amd64 (ltsc2025) |
| `wyga/web-runtime-go:2022` | linux/amd64, linux/arm64, windows/amd64 (ltsc2022) |
| `wyga/web-runtime-go:2019` | linux/amd64, linux/arm64, windows/amd64 (ltsc2019) |

For demo purposes only.
