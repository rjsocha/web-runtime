# java

Runtime-info page over HTTP on the JDK alone - `com.sun.net.httpserver`, no
build tool, no dependencies. Requests are served on virtual threads.

```sh
make payload      # wyga/web-runtime-java-payload:latest - app.jar on scratch
make multi        # wyga/web-runtime-java:2022, :2025, :latest
make run
```

Bytecode is platform-neutral, so the payload is built once and joined to a JRE
image per Windows base, exactly as in the dotnet project. Temurin publishes no
nanoserver image for Windows Server 2019, so the tag list stops at 2022.
