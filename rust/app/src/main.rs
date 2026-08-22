use http_body_util::Full;
use hyper::body::{Bytes, Incoming};
use hyper::server::conn::http1;
use hyper::service::service_fn;
use hyper::{Request, Response};
use hyper_util::rt::TokioIo;
use std::convert::Infallible;
use std::net::SocketAddr;
use std::sync::OnceLock;
use std::time::Instant;
use tokio::net::TcpListener;

type Row = (String, String);

static STARTED: OnceLock<Instant> = OnceLock::new();

#[tokio::main]
async fn main() {
    STARTED.set(Instant::now()).ok();

    let port: u16 = std::env::var("PORT").ok().and_then(|v| v.parse().ok()).unwrap_or(8080);
    let address = SocketAddr::from(([0, 0, 0, 0], port));

    let listener = match TcpListener::bind(address).await {
        Ok(listener) => listener,
        Err(failure) => {
            eprintln!("cannot listen on {address}: {failure}");
            std::process::exit(1);
        }
    };

    tokio::spawn(shutdown());

    loop {
        let (stream, remote) = match listener.accept().await {
            Ok(accepted) => accepted,
            Err(_) => continue,
        };

        let local = stream.local_addr().ok();

        tokio::spawn(async move {
            let service = service_fn(move |request| serve(request, remote, local));
            let _ = http1::Builder::new().serve_connection(TokioIo::new(stream), service).await;
        });
    }
}

/// Nothing terminates a process that runs as PID 1 unless it handles the
/// signal itself, so docker stop would wait for its timeout and then kill it.
async fn shutdown() {
    #[cfg(unix)]
    {
        use tokio::signal::unix::{signal, SignalKind};
        let mut term = signal(SignalKind::terminate()).expect("SIGTERM");
        let mut int = signal(SignalKind::interrupt()).expect("SIGINT");

        tokio::select! {
            _ = term.recv() => {}
            _ = int.recv() => {}
        }
    }

    #[cfg(not(unix))]
    {
        let _ = tokio::signal::ctrl_c().await;
    }

    std::process::exit(0);
}

async fn serve(
    request: Request<Incoming>,
    remote: SocketAddr,
    local: Option<SocketAddr>,
) -> Result<Response<Full<Bytes>>, Infallible> {
    let path = request.uri().path().to_string();
    let environment = path.starts_with("/env");
    let verbose = environment || path.starts_with("/verbose");

    let body = report(&request, remote, local, verbose, environment);

    Ok(Response::builder()
        .header("content-type", "text/plain; charset=utf-8")
        .header("connection", "close")
        .body(Full::new(Bytes::from(body)))
        .expect("response"))
}

fn report(
    request: &Request<Incoming>,
    remote: SocketAddr,
    local: Option<SocketAddr>,
    verbose: bool,
    with_environment: bool,
) -> String {
    let mut runtime: Vec<Row> = vec![
        row("hostname", hostname()),
        row("uptime", uptime()),
        row("framework", "hyper on tokio".to_string()),
        row("runtime identifier", format!("{}-{}", std::env::consts::OS, std::env::consts::ARCH)),
        row("os", os_description()),
        row("architecture", std::env::consts::ARCH.to_string()),
        row("processors", processors()),
        row("crt", crt()),
    ];

    if verbose {
        runtime.push(row("pointer width", std::mem::size_of::<usize>().saturating_mul(8).to_string()));
        runtime.push(row("debug assertions", cfg!(debug_assertions).to_string()));
    } else {
        runtime.push(row("ram", total_memory()));
    }

    if let Ok(node) = std::env::var("RUNTIME_NODE") {
        if !node.is_empty() {
            let details: Vec<String> = ["RUNTIME_NODE_ID", "RUNTIME_TASK", "RUNTIME_SLOT"]
                .iter()
                .filter_map(|name| std::env::var(name).ok())
                .filter(|value| !value.is_empty())
                .collect();

            let value = if details.is_empty() {
                node
            } else {
                format!("{} ({})", node, details.join(" / "))
            };

            runtime.insert(0, row("node", value));
        }
    }

    let mut out = section("runtime", &runtime);

    if verbose {
        out += &section(
            "process",
            &[
                row("pid", std::process::id().to_string()),
                row("executable", std::env::current_exe()
                    .map(|path| path.display().to_string())
                    .unwrap_or_else(|_| "-".into())),
                row("working directory", std::env::current_dir()
                    .map(|path| path.display().to_string())
                    .unwrap_or_else(|_| "-".into())),
                row("command line", std::env::args().collect::<Vec<_>>().join(" ")),
            ],
        );
    }

    out += &section("network", &interfaces());

    out += &section(
        "request",
        &[
            row("method", request.method().to_string()),
            row("path", request.uri().path_and_query()
                .map(|value| value.to_string())
                .unwrap_or_else(|| "/".into())),
            row("protocol", format!("{:?}", request.version())),
            row("scheme", "http".to_string()),
            row("host header", header(request, "host")),
            row("remote", endpoint(Some(remote))),
            row("local", endpoint(local)),
        ],
    );

    if verbose {
        let mut headers: Vec<Row> = request
            .headers()
            .iter()
            .map(|(name, value)| row(name.as_str(), value.to_str().unwrap_or("").to_string()))
            .collect();
        headers.sort();
        out += &section("headers", &headers);
    }

    if with_environment {
        let mut rows: Vec<Row> = std::env::vars().map(|(name, value)| (name, value)).collect();
        rows.sort();
        out += &section("environment", &rows);
    }

    out
}

fn row(key: &str, value: String) -> Row {
    (key.to_string(), value)
}

fn section(title: &str, rows: &[Row]) -> String {
    let width = rows.iter().map(|(key, _)| key.len().min(30)).max().unwrap_or(0);

    let mut out = format!("[{title}]\n");
    for (key, value) in rows {
        let value = value.replace(['\r', '\n'], " ");
        out += &format!("{key:<width$}  {value}\n");
    }

    out + "\n"
}

fn header(request: &Request<Incoming>, name: &str) -> String {
    request
        .headers()
        .get(name)
        .and_then(|value| value.to_str().ok())
        .unwrap_or("-")
        .to_string()
}

fn endpoint(address: Option<SocketAddr>) -> String {
    match address {
        Some(SocketAddr::V4(value)) => format!("{}:{}", value.ip(), value.port()),
        Some(SocketAddr::V6(value)) => match value.ip().to_ipv4_mapped() {
            Some(ip) => format!("{}:{}", ip, value.port()),
            None => format!("[{}]:{}", value.ip(), value.port()),
        },
        None => "-".to_string(),
    }
}

fn uptime() -> String {
    let seconds = STARTED.get().map(|start| start.elapsed().as_secs()).unwrap_or(0);
    format!("{}.{:02}:{:02}:{:02}", seconds / 86400, seconds % 86400 / 3600, seconds % 3600 / 60, seconds % 60)
}

fn processors() -> String {
    std::thread::available_parallelism()
        .map(|value| value.get().to_string())
        .unwrap_or_else(|_| "-".into())
}

fn hostname() -> String {
    if let Ok(name) = std::env::var("HOSTNAME") {
        if !name.is_empty() {
            return name;
        }
    }

    if let Ok(name) = std::env::var("COMPUTERNAME") {
        if !name.is_empty() {
            return name;
        }
    }

    std::fs::read_to_string("/etc/hostname")
        .map(|name| name.trim().to_string())
        .unwrap_or_else(|_| "-".into())
}

fn os_description() -> String {
    if cfg!(windows) {
        return "Windows".to_string();
    }

    if let Ok(release) = std::fs::read_to_string("/etc/os-release") {
        for line in release.lines() {
            if let Some(name) = line.strip_prefix("PRETTY_NAME=") {
                return name.trim_matches('"').to_string();
            }
        }
    }

    std::env::consts::OS.to_string()
}

#[cfg(windows)]
fn crt() -> String {
    unsafe extern "system" {
        fn GetModuleHandleA(name: *const u8) -> *mut core::ffi::c_void;
    }

    let loaded = unsafe {
        if !GetModuleHandleA(c"ucrtbase.dll".as_ptr() as *const u8).is_null() {
            "ucrtbase.dll"
        } else if !GetModuleHandleA(c"msvcrt.dll".as_ptr() as *const u8).is_null() {
            "msvcrt.dll"
        } else {
            "unknown"
        }
    };

    format!("{loaded}, windows-gnu")
}

#[cfg(not(windows))]
fn crt() -> String {
    if let Ok(maps) = std::fs::read_to_string("/proc/self/maps") {
        if maps.contains("/libc.so") {
            return "glibc, dynamic".to_string();
        }
        if maps.contains("ld-musl") || maps.contains("libc.musl") {
            return "musl, dynamic".to_string();
        }
    }

    "musl, static".to_string()
}

fn total_memory() -> String {
    #[cfg(windows)]
    {
        #[repr(C)]
        struct MemoryStatus {
            length: u32,
            memory_load: u32,
            total_physical: u64,
            available_physical: u64,
            total_page_file: u64,
            available_page_file: u64,
            total_virtual: u64,
            available_virtual: u64,
            available_extended_virtual: u64,
        }

        unsafe extern "system" {
            fn GlobalMemoryStatusEx(buffer: *mut MemoryStatus) -> i32;
        }

        let mut status = MemoryStatus {
            length: std::mem::size_of::<MemoryStatus>() as u32,
            memory_load: 0,
            total_physical: 0,
            available_physical: 0,
            total_page_file: 0,
            available_page_file: 0,
            total_virtual: 0,
            available_virtual: 0,
            available_extended_virtual: 0,
        };

        if unsafe { GlobalMemoryStatusEx(&mut status) } != 0 {
            return mb(status.total_physical);
        }

        return "-".to_string();
    }

    #[cfg(not(windows))]
    {
        for path in ["/sys/fs/cgroup/memory.max", "/sys/fs/cgroup/memory/limit_in_bytes"] {
            if let Ok(content) = std::fs::read_to_string(path) {
                let value = content.trim();
                if value == "max" {
                    break;
                }
                if let Ok(limit) = value.parse::<u64>() {
                    if limit < (1 << 62) {
                        return mb(limit);
                    }
                }
            }
        }

        if let Ok(meminfo) = std::fs::read_to_string("/proc/meminfo") {
            for line in meminfo.lines() {
                if let Some(rest) = line.strip_prefix("MemTotal:") {
                    if let Some(kilobytes) = rest.split_whitespace().next() {
                        if let Ok(value) = kilobytes.parse::<u64>() {
                            return mb(value * 1024);
                        }
                    }
                }
            }
        }

        "-".to_string()
    }
}

fn interfaces() -> Vec<Row> {
    let mut byname: std::collections::BTreeMap<String, Vec<String>> = Default::default();

    if let Ok(list) = if_addrs::get_if_addrs() {
        for interface in list {
            let prefix = match &interface.addr {
                if_addrs::IfAddr::V4(v4) => u32::from(v4.netmask).count_ones(),
                if_addrs::IfAddr::V6(v6) => v6.netmask.octets().iter().map(|byte| byte.count_ones()).sum(),
            };

            byname
                .entry(interface.name.clone())
                .or_default()
                .push(format!("{}/{}", interface.addr.ip(), prefix));
        }
    }

    byname.into_iter().map(|(name, addresses)| (name, addresses.join(", "))).collect()
}

fn mb(bytes: u64) -> String {
    format!("{:.1} MB", bytes as f64 / 1024.0 / 1024.0)
}
