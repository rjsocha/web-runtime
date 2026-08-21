# java-javalin

The same report as [java](../java/), served by Javalin on Jetty instead of the
JDK's own HTTP server. Maven builds a shaded jar; the packaging is unchanged -
the jar is platform-neutral, so one payload feeds a JRE image per Windows base.

```sh
make payload      # wyga/web-runtime-java-javalin-payload:latest
make multi        # wyga/web-runtime-java-javalin:2022, :2025, :latest
make run
```

The point of comparison is what a framework costs: jar size, image size, start
time and memory against the JDK-only version.
