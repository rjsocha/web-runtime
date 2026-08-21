# dotnet

Runtime-info page over HTTP, packaged as two images: `wyga/web-runtime-dotnet-payload` with the
compiled app, and a deployable that is a .NET runtime with that app copied in.
The app is portable IL, so one app image feeds both a Linux and a Windows
deployable.

```sh
make payload      # wyga/web-runtime-dotnet-payload:latest
make multi        # wyga/web-runtime-dotnet:2022, :2025, :latest
make run
```

Both targets push: a Linux daemon will not store a Windows image, so the
builds go through a `docker-container` builder straight to the registry.

For demo purposes only - a toy for playing with multi-platform Docker Swarm.
Nothing here belongs in production, and .NET 10 has no reason to run on a
Windows node in the first place.
