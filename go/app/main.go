package main

import (
	"fmt"
	"io"
	"net"
	"net/http"
	"net/netip"
	"os"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"time"
)

var started = time.Now()

type row struct {
	key   string
	value string
}

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	http.HandleFunc("/", handle)

	if err := http.ListenAndServe(":"+port, nil); err != nil {
		fmt.Fprintf(os.Stderr, "cannot listen on :%s: %v\n", port, err)
		os.Exit(1)
	}
}

func handle(w http.ResponseWriter, r *http.Request) {
	environment := strings.HasPrefix(r.URL.Path, "/env")
	verbose := environment || strings.HasPrefix(r.URL.Path, "/verbose")

	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	w.Header().Set("Connection", "close")

	io.WriteString(w, report(r, verbose, environment))
}

func report(r *http.Request, verbose, withEnvironment bool) string {
	var out strings.Builder

	rt := []row{
		{"hostname", hostname()},
		{"uptime", time.Since(started).Truncate(time.Second).String()},
		{"framework", runtime.Version()},
		{"runtime identifier", runtime.GOOS + "-" + runtime.GOARCH},
		{"os", osDescription()},
		{"architecture", runtime.GOARCH},
		{"processors", strconv.Itoa(runtime.NumCPU())},
	}

	if verbose {
		rt = append(rt,
			row{"gomaxprocs", strconv.Itoa(runtime.GOMAXPROCS(0))},
			row{"compiler", runtime.Compiler})
	} else {
		rt = append(rt, row{"ram", memoryLimit()})
	}

	out.WriteString(section("runtime", rt))

	if verbose {
		wd, _ := os.Getwd()
		exe, _ := os.Executable()

		out.WriteString(section("process", []row{
			{"pid", strconv.Itoa(os.Getpid())},
			{"executable", exe},
			{"working directory", wd},
			{"command line", strings.Join(os.Args, " ")},
			{"goroutines", strconv.Itoa(runtime.NumGoroutine())},
		}))

		var m runtime.MemStats
		runtime.ReadMemStats(&m)

		out.WriteString(section("memory", []row{
			{"heap in use", mb(m.HeapInuse)},
			{"heap reserved", mb(m.HeapSys)},
			{"stack in use", mb(m.StackInuse)},
			{"from the os", mb(m.Sys)},
			{"allocated in total", mb(m.TotalAlloc)},
			{"available", memoryLimit()},
			{"next gc", mb(m.NextGC)},
			{"collections", strconv.FormatUint(uint64(m.NumGC), 10)},
		}))
	}

	out.WriteString(section("network", interfaces()))
	out.WriteString(section("request", request(r)))

	if verbose {
		names := make([]string, 0, len(r.Header))
		for name := range r.Header {
			names = append(names, name)
		}
		sort.Strings(names)

		headers := make([]row, 0, len(names))
		for _, name := range names {
			headers = append(headers, row{name, strings.Join(r.Header[name], ", ")})
		}

		out.WriteString(section("headers", headers))
	}

	if withEnvironment {
		env := os.Environ()
		sort.Strings(env)

		rows := make([]row, 0, len(env))
		for _, entry := range env {
			name, value, _ := strings.Cut(entry, "=")
			rows = append(rows, row{name, value})
		}

		out.WriteString(section("environment", rows))
	}

	return out.String()
}

func request(r *http.Request) []row {
	scheme := "http"
	if r.TLS != nil {
		scheme = "https"
	}

	local := ""
	if addr, ok := r.Context().Value(http.LocalAddrContextKey).(net.Addr); ok {
		local = endpoint(addr.String())
	}

	return []row{
		{"method", r.Method},
		{"path", r.URL.RequestURI()},
		{"protocol", r.Proto},
		{"scheme", scheme},
		{"host header", r.Host},
		{"remote", endpoint(r.RemoteAddr)},
		{"local", local},
	}
}

func interfaces() []row {
	found, err := net.Interfaces()
	if err != nil {
		return nil
	}

	rows := []row{}
	for _, iface := range found {
		if iface.Flags&net.FlagUp == 0 {
			continue
		}

		addrs, err := iface.Addrs()
		if err != nil || len(addrs) == 0 {
			continue
		}

		list := make([]string, 0, len(addrs))
		for _, addr := range addrs {
			list = append(list, addr.String())
		}

		rows = append(rows, row{iface.Name, strings.Join(list, ", ")})
	}

	return rows
}

func section(title string, rows []row) string {
	width := 0
	for _, r := range rows {
		if n := len(r.key); n > width && n <= 30 {
			width = n
		}
	}

	var out strings.Builder
	fmt.Fprintf(&out, "[%s]\n", title)

	replace := strings.NewReplacer("\r\n", " ", "\n", " ", "\r", " ")
	for _, r := range rows {
		fmt.Fprintf(&out, "%-*s  %s\n", width, r.key, replace.Replace(r.value))
	}

	out.WriteString("\n")
	return out.String()
}

func endpoint(addr string) string {
	host, port, err := net.SplitHostPort(addr)
	if err != nil {
		return addr
	}

	ip, err := netip.ParseAddr(host)
	if err != nil {
		return addr
	}

	ip = ip.Unmap()
	if ip.Is4() {
		return ip.String() + ":" + port
	}

	return "[" + ip.String() + "]:" + port
}

func hostname() string {
	name, err := os.Hostname()
	if err != nil {
		return "-"
	}
	return name
}

func osDescription() string {
	release, err := os.ReadFile("/etc/os-release")
	if err != nil {
		return runtime.GOOS
	}

	for _, line := range strings.Split(string(release), "\n") {
		if name, ok := strings.CutPrefix(line, "PRETTY_NAME="); ok {
			return strings.Trim(name, `"`)
		}
	}

	return runtime.GOOS
}

func memoryLimit() string {
	for _, path := range []string{"/sys/fs/cgroup/memory.max", "/sys/fs/cgroup/memory/limit_in_bytes"} {
		content, err := os.ReadFile(path)
		if err != nil {
			continue
		}

		value := strings.TrimSpace(string(content))
		if value == "max" {
			break
		}

		limit, err := strconv.ParseUint(value, 10, 64)
		if err == nil && limit < 1<<62 {
			return mb(limit)
		}
	}

	if total := memTotal(); total > 0 {
		return mb(total)
	}

	return "-"
}

func memTotal() uint64 {
	content, err := os.ReadFile("/proc/meminfo")
	if err != nil {
		return 0
	}

	for _, line := range strings.Split(string(content), "\n") {
		if !strings.HasPrefix(line, "MemTotal:") {
			continue
		}

		fields := strings.Fields(line)
		if len(fields) < 2 {
			return 0
		}

		kb, err := strconv.ParseUint(fields[1], 10, 64)
		if err != nil {
			return 0
		}

		return kb * 1024
	}

	return 0
}

func mb(bytes uint64) string {
	return fmt.Sprintf("%.1f MB", float64(bytes)/1024/1024)
}
