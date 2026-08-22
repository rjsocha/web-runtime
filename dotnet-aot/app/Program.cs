using System.Diagnostics;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Runtime;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.AspNetCore.Hosting.Server;
using Microsoft.AspNetCore.Hosting.Server.Features;

var builder = WebApplication.CreateSlimBuilder(args);
var app = builder.Build();

app.Map("/{**rest}", (HttpContext ctx) =>
{
    var path = ctx.Request.Path;
    var environment = path.StartsWithSegments("/env");
    var verbose = environment || path.StartsWithSegments("/verbose");

    // One request per connection: the ingress mesh balances connections, not
    // requests, so a kept-alive browser would keep landing on one replica.
    ctx.Response.Headers.Connection = "close";

    return Results.Text(Report(ctx, verbose, environment), "text/plain; charset=utf-8");
});

app.Run();

static string Report(HttpContext ctx, bool verbose, bool withEnvironment)
{
    var proc = Process.GetCurrentProcess();
    var req = ctx.Request;
    var conn = ctx.Connection;
    var host = ctx.RequestServices.GetRequiredService<IHostEnvironment>();
    var addresses = ctx.RequestServices.GetService<IServer>()?
        .Features.Get<IServerAddressesFeature>()?.Addresses ?? [];
    var gc = GC.GetGCMemoryInfo();

    var report = new StringBuilder();

    var runtime = new List<(string, string)>
    {
        ("hostname", Environment.MachineName),
        ("uptime", (DateTime.Now - proc.StartTime).ToString(@"d\.hh\:mm\:ss")),
        ("framework", RuntimeInformation.FrameworkDescription
            + (RuntimeFeature.IsDynamicCodeCompiled ? "" : " - AOT")),
        ("runtime identifier", RuntimeInformation.RuntimeIdentifier),
        ("os", RuntimeInformation.OSDescription),
        ("architecture", $"{RuntimeInformation.ProcessArchitecture} on {RuntimeInformation.OSArchitecture}"),
        ("processors", Environment.ProcessorCount.ToString()),
    };

    if (verbose)
    {
        runtime.Add(("server gc", GCSettings.IsServerGC.ToString()));
        runtime.Add(("gc mode", GCSettings.LatencyMode.ToString()));
    }
    else
    {
        runtime.Add(("ram", Mb(gc.TotalAvailableMemoryBytes)));
    }

    var node = Environment.GetEnvironmentVariable("RUNTIME_NODE");
    if (!string.IsNullOrEmpty(node))
    {
        var details = new[] { "RUNTIME_NODE_ID", "RUNTIME_TASK", "RUNTIME_SLOT" }
            .Select(Environment.GetEnvironmentVariable)
            .Where(value => !string.IsNullOrEmpty(value));

        runtime.Insert(0, ("node", details.Any() ? $"{node} ({string.Join(" / ", details)})" : node));
    }

    report.Append(Section("runtime", runtime));

    if (verbose)
    {
        report.Append(Section("process",
        [
            ("pid", Environment.ProcessId.ToString()),
            ("user", Environment.UserName),
            ("threads", proc.Threads.Count.ToString()),
            ("base directory", AppContext.BaseDirectory),
            ("content root", host.ContentRootPath),
            ("environment", host.EnvironmentName),
            ("listening on", string.Join(", ", addresses)),
            ("command line", Environment.CommandLine),
        ]));

        report.Append(Section("memory",
        [
            ("working set", Mb(proc.WorkingSet64)),
            ("private", Mb(proc.PrivateMemorySize64)),
            ("gc heap", Mb(GC.GetTotalMemory(false))),
            ("gc committed", Mb(gc.TotalCommittedBytes)),
            ("allocated in total", Mb(GC.GetTotalAllocatedBytes())),
            ("available", Mb(gc.TotalAvailableMemoryBytes)),
            ("high load threshold", Mb(gc.HighMemoryLoadThresholdBytes)),
            ("collections", $"gen0 {GC.CollectionCount(0)}, gen1 {GC.CollectionCount(1)}, gen2 {GC.CollectionCount(2)}"),
        ]));

    }

    report.Append(Section("network", NetworkInterface.GetAllNetworkInterfaces()
        .Where(n => n.OperationalStatus == OperationalStatus.Up)
        .Select(n => (n.Name, Addresses: string.Join(", ", n.GetIPProperties().UnicastAddresses
            .Select(a => $"{a.Address}/{a.PrefixLength}"))))
        .Where(n => n.Addresses.Length > 0)));

    report.Append(Section("request",
    [
        ("method", req.Method),
        ("path", req.Path + req.QueryString),
        ("protocol", req.Protocol),
        ("scheme", req.Scheme),
        ("host header", req.Host.ToString()),
        ("remote", Endpoint(conn.RemoteIpAddress, conn.RemotePort)),
        ("local", Endpoint(conn.LocalIpAddress, conn.LocalPort)),
    ]));

    if (verbose)
        report.Append(Section("headers", req.Headers
            .OrderBy(h => h.Key, StringComparer.OrdinalIgnoreCase)
            .Select(h => (h.Key, string.Join(", ", h.Value.Select(v => v ?? ""))))));

    if (withEnvironment)
        report.Append(Section("environment", Environment.GetEnvironmentVariables()
            .Cast<System.Collections.DictionaryEntry>()
            .Select(e => ((string)e.Key, (string?)e.Value ?? ""))
            .OrderBy(e => e.Item1, StringComparer.OrdinalIgnoreCase)));

    return report.ToString();
}

static string Section(string title, IEnumerable<(string Key, string Value)> rows)
{
    var list = rows.ToList();
    var width = list.Count == 0 ? 0 : Math.Min(list.Max(r => r.Key.Length), 30);
    var section = new StringBuilder($"[{title}]\n");
    foreach (var (key, value) in list)
        section.Append(key.PadRight(width)).Append("  ").Append(value.ReplaceLineEndings(" ")).Append('\n');
    return section.Append('\n').ToString();
}

static string Endpoint(IPAddress? address, int port)
{
    if (address is null) return "-";

    var plain = address.IsIPv4MappedToIPv6 ? address.MapToIPv4() : address;

    return plain.AddressFamily == AddressFamily.InterNetwork
        ? $"{plain}:{port}"
        : $"[{plain}]:{port}";
}

static string Mb(long bytes) => $"{bytes / 1024.0 / 1024.0:0.#} MB";
