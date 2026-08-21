import com.sun.management.OperatingSystemMXBean;
import io.javalin.Javalin;
import io.javalin.http.Context;

import jakarta.servlet.http.HttpServletRequest;

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.lang.management.RuntimeMXBean;
import java.net.InetAddress;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

public class Main {

    record Row(String key, String value) {}

    public static void main(String[] args) {
        int port = Integer.parseInt(System.getenv().getOrDefault("PORT", "8080"));

        Javalin.create(config -> {
            config.startup.showJavalinBanner = false;
            config.routes.get("/", Main::handle);
            config.routes.get("/<path>", Main::handle);
        }).start(port);
    }

    static void handle(Context ctx) {
        String path = ctx.path();
        boolean environment = path.startsWith("/env");
        boolean verbose = environment || path.startsWith("/verbose");

        ctx.contentType("text/plain; charset=utf-8");
        ctx.header("Connection", "close");
        ctx.result(report(ctx, verbose, environment));
    }

    static String report(Context ctx, boolean verbose, boolean withEnvironment) {
        RuntimeMXBean runtime = ManagementFactory.getRuntimeMXBean();
        OperatingSystemMXBean os = (OperatingSystemMXBean) ManagementFactory.getOperatingSystemMXBean();

        StringBuilder out = new StringBuilder();

        List<Row> rt = new ArrayList<>(List.of(
                new Row("hostname", hostname()),
                new Row("uptime", uptime(runtime.getUptime())),
                new Row("framework", "Javalin " + version(Javalin.class) + " on Jetty " + jetty()),
                new Row("runtime", System.getProperty("java.runtime.name") + " "
                        + System.getProperty("java.runtime.version")),
                new Row("runtime identifier", identifier()),
                new Row("os", System.getProperty("os.name") + " " + System.getProperty("os.version")),
                new Row("architecture", System.getProperty("os.arch")),
                new Row("processors", String.valueOf(Runtime.getRuntime().availableProcessors()))));

        if (verbose) {
            rt.add(new Row("vm", System.getProperty("java.vm.name") + " " + System.getProperty("java.vm.version")));
            rt.add(new Row("vendor", System.getProperty("java.vendor")));
        } else {
            rt.add(new Row("ram", mb(os.getTotalMemorySize())));
        }

        String node = System.getenv("RUNTIME_NODE");
        if (node != null && !node.isEmpty()) {
            List<String> details = new ArrayList<>();
            for (String name : List.of("RUNTIME_NODE_ID", "RUNTIME_TASK", "RUNTIME_SLOT")) {
                String value = System.getenv(name);
                if (value != null && !value.isEmpty()) {
                    details.add(value);
                }
            }

            rt.add(0, new Row("node", details.isEmpty() ? node : node + " (" + String.join(" / ", details) + ")"));
        }

        out.append(section("runtime", rt));

        if (verbose) {
            out.append(section("process", List.of(
                    new Row("pid", String.valueOf(ProcessHandle.current().pid())),
                    new Row("user", System.getProperty("user.name")),
                    new Row("threads", String.valueOf(ManagementFactory.getThreadMXBean().getThreadCount())),
                    new Row("java home", System.getProperty("java.home")),
                    new Row("working directory", System.getProperty("user.dir")),
                    new Row("command line", String.valueOf(System.getProperty("sun.java.command"))),
                    new Row("vm arguments", String.join(" ", runtime.getInputArguments())))));

            MemoryMXBean memory = ManagementFactory.getMemoryMXBean();

            List<String> collections = new ArrayList<>();
            for (GarbageCollectorMXBean collector : ManagementFactory.getGarbageCollectorMXBeans()) {
                collections.add(collector.getName() + " " + collector.getCollectionCount());
            }

            out.append(section("memory", List.of(
                    new Row("heap used", mb(memory.getHeapMemoryUsage().getUsed())),
                    new Row("heap committed", mb(memory.getHeapMemoryUsage().getCommitted())),
                    new Row("heap max", mb(memory.getHeapMemoryUsage().getMax())),
                    new Row("non heap used", mb(memory.getNonHeapMemoryUsage().getUsed())),
                    new Row("available", mb(os.getTotalMemorySize())),
                    new Row("free", mb(os.getFreeMemorySize())),
                    new Row("collections", String.join(", ", collections)))));
        }

        out.append(section("network", interfaces()));
        out.append(section("request", request(ctx)));

        if (verbose) {
            List<Row> headers = new ArrayList<>();
            Map<String, String> sorted = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
            sorted.putAll(ctx.headerMap());

            for (Map.Entry<String, String> header : sorted.entrySet()) {
                headers.add(new Row(header.getKey(), header.getValue()));
            }

            out.append(section("headers", headers));
        }

        if (withEnvironment) {
            List<Row> rows = new ArrayList<>();
            Map<String, String> sorted = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
            sorted.putAll(System.getenv());

            for (Map.Entry<String, String> entry : sorted.entrySet()) {
                rows.add(new Row(entry.getKey(), entry.getValue()));
            }

            out.append(section("environment", rows));
        }

        return out.toString();
    }

    static List<Row> request(Context ctx) {
        HttpServletRequest req = ctx.req();
        String query = req.getQueryString();

        return List.of(
                new Row("method", req.getMethod()),
                new Row("path", req.getRequestURI() + (query == null ? "" : "?" + query)),
                new Row("protocol", req.getProtocol()),
                new Row("scheme", req.getScheme()),
                new Row("host header", String.valueOf(req.getHeader("Host"))),
                new Row("remote", endpoint(req.getRemoteAddr(), req.getRemotePort())),
                new Row("local", endpoint(req.getLocalAddr(), req.getLocalPort())));
    }

    static List<Row> interfaces() {
        List<Row> rows = new ArrayList<>();

        try {
            for (NetworkInterface iface : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                if (!iface.isUp()) {
                    continue;
                }

                List<String> addresses = new ArrayList<>();
                for (InterfaceAddress address : iface.getInterfaceAddresses()) {
                    addresses.add(address.getAddress().getHostAddress() + "/" + address.getNetworkPrefixLength());
                }

                if (!addresses.isEmpty()) {
                    rows.add(new Row(iface.getName(), String.join(", ", addresses)));
                }
            }
        } catch (SocketException ignored) {
            // an interface list we cannot read is reported as no interfaces
        }

        return rows;
    }

    static String section(String title, List<Row> rows) {
        int width = 0;
        for (Row row : rows) {
            width = Math.max(width, Math.min(row.key().length(), 30));
        }

        StringBuilder out = new StringBuilder("[" + title + "]\n");
        for (Row row : rows) {
            out.append(String.format("%-" + Math.max(width, 1) + "s  %s\n",
                    row.key(), String.valueOf(row.value()).replaceAll("\\R", " ")));
        }

        return out.append("\n").toString();
    }

    static String endpoint(String address, int port) {
        if (address == null) {
            return "-";
        }

        return address.contains(":") ? "[" + address + "]:" + port : address + ":" + port;
    }

    static String hostname() {
        try {
            return InetAddress.getLocalHost().getHostName();
        } catch (Exception ignored) {
            String name = System.getenv("HOSTNAME");
            return name == null ? "-" : name;
        }
    }

    static String version(Class<?> type) {
        String value = type.getPackage() == null ? null : type.getPackage().getImplementationVersion();
        return value == null ? "-" : value;
    }

    static String jetty() {
        try {
            return String.valueOf(Class.forName("org.eclipse.jetty.util.Jetty").getField("VERSION").get(null));
        } catch (Throwable ignored) {
            return "-";
        }
    }

    static String identifier() {
        String os = System.getProperty("os.name").toLowerCase().startsWith("windows") ? "windows" : "linux";
        return os + "-" + System.getProperty("os.arch");
    }

    static String uptime(long milliseconds) {
        long seconds = milliseconds / 1000;
        return String.format("%d.%02d:%02d:%02d",
                seconds / 86400, seconds % 86400 / 3600, seconds % 3600 / 60, seconds % 60);
    }

    static String mb(long bytes) {
        return bytes < 0 ? "-" : String.format("%.1f MB", bytes / 1024.0 / 1024.0);
    }
}
