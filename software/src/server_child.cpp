#include "server_child.h"
#include "server_common.h"

#include <libwebsockets.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <grp.h>
#include <map>
#include <pwd.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

constexpr size_t RingSize = 600;

struct Sample { int64_t ts; double value; };
using RingMap = std::vector<std::pair<std::string, std::deque<Sample>>>;

std::deque<Sample> &ringFor(RingMap &rings, const std::string &path)
{
    for (auto &entry : rings) {
        if (entry.first == path)
            return entry.second;
    }
    rings.emplace_back(path, std::deque<Sample>());
    return rings.back().second;
}

struct Session {
    std::deque<std::string> outgoing;
};

RingMap *gRings = nullptr;
std::vector<struct lws *> *gClients = nullptr;
// Sessions are kept here rather than in lws's per-session `user` data: lws
// allocates that as a zeroed raw buffer and never runs a constructor, which
// is undefined behaviour for a type holding a std::deque.
std::map<struct lws *, Session> *gSessions = nullptr;

void writeStatus(int fd, const char *msg)
{
    write(fd, msg, strlen(msg));
    // Also to stderr (redirected to a log file by the parent before fork)
    // -- the parent's stdout print is gone the instant curses repaints.
    write(STDERR_FILENO, msg, strlen(msg));
}

// setgroups()/setuid() ordering matters: dropping the uid first would leave
// the process without permission to drop supplementary groups afterwards.
bool dropPrivileges()
{
    if (geteuid() != 0)
        return true; // already unprivileged (e.g. desktop dev build)

    struct passwd *pw = getpwnam(ServerProtocol::DropPrivilegesUser);
    if (pw == nullptr)
        return false;

    if (setgroups(0, nullptr) != 0)
        return false;
    if (setgid(pw->pw_gid) != 0)
        return false;
    if (setuid(pw->pw_uid) != 0)
        return false;

    // setuid() can silently no-op under some sandboxed/capability setups, so
    // confirm the drop actually stuck rather than trusting the return codes.
    return getuid() == pw->pw_uid && geteuid() == pw->pw_uid;
}

// Only needed once per new client connection, not per sample -- rebuilding
// this from potentially hundreds of samples per series on every single
// incoming update was real, unnecessary work sitting in the hot path.
std::string buildBacklogJson(const RingMap &rings)
{
    std::string json = R"({"type":"backfill","series":{)";
    bool firstSeries = true;
    for (const auto &entry : rings) {
        if (!firstSeries) json += ",";
        firstSeries = false;
        // D-Bus object paths are restricted to [A-Za-z0-9_/], so no JSON
        // escaping is needed for entry.first.
        json += "\"" + entry.first + "\":[";
        bool firstSample = true;
        for (const auto &sample : entry.second) {
            if (!firstSample) json += ",";
            firstSample = false;
            json += "[" + std::to_string(sample.ts) + "," + std::to_string(sample.value) + "]";
        }
        json += "]";
    }
    json += "}}";
    return json;
}

// D-Bus object paths are restricted to [A-Za-z0-9_/], so no JSON escaping is
// needed for any path in `paths` (same assumption buildBacklogJson() makes).
std::string buildOrderJson(const std::vector<std::string> &paths)
{
    std::string json = R"({"type":"order","paths":[)";
    for (size_t i = 0; i < paths.size(); ++i) {
        if (i) json += ",";
        json += "\"" + paths[i] + "\"";
    }
    json += "]}";
    return json;
}

void broadcastToAllSessions(const std::string &msg)
{
    for (struct lws *wsi : *gClients) {
        auto it = gSessions->find(wsi);
        if (it == gSessions->end())
            continue;
        it->second.outgoing.push_back(msg);
        lws_callback_on_writable(wsi);
    }
}

int wsCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED: {
        gClients->push_back(wsi);
        Session &session = (*gSessions)[wsi];
        session.outgoing.push_back(buildBacklogJson(*gRings));
        lws_callback_on_writable(wsi);
        break;
    }
    case LWS_CALLBACK_CLOSED:
        gClients->erase(std::remove(gClients->begin(), gClients->end(), wsi), gClients->end());
        gSessions->erase(wsi);
        break;
    case LWS_CALLBACK_SERVER_WRITEABLE: {
        auto it = gSessions->find(wsi);
        if (it != gSessions->end() && !it->second.outgoing.empty()) {
            const std::string &msg = it->second.outgoing.front();
            std::vector<unsigned char> buf(LWS_PRE + msg.size());
            memcpy(buf.data() + LWS_PRE, msg.data(), msg.size());
            lws_write(wsi, buf.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
            it->second.outgoing.pop_front();
            if (!it->second.outgoing.empty())
                lws_callback_on_writable(wsi);
        }
        break;
    }
    default:
        break;
    }
    (void)user;
    (void)in;
    (void)len;
    return 0;
}

} // namespace

[[noreturn]] void runServerChild(int pipeFd, int port)
{
    signal(SIGPIPE, SIG_IGN);

    if (!dropPrivileges()) {
        char msg[128];
        snprintf(msg, sizeof(msg), "ERR failed to drop privileges to '%s'\n", ServerProtocol::DropPrivilegesUser);
        writeStatus(pipeFd, msg);
        _exit(1);
    }

    RingMap rings;
    std::vector<struct lws *> clients;
    std::map<struct lws *, Session> sessions;
    gRings = &rings;
    gClients = &clients;
    gSessions = &sessions;

    // No HTTP mount: this process only serves the WebSocket data feed. The
    // page itself (software/graph.html, one possible consumer) is served
    // however the user chooses -- opened locally, or hosted by something
    // like nginx on the GX.
    struct lws_protocols protocols[] = {
        { "http", lws_callback_http_dummy, 0, 0 },
        // Session state lives in gSessions, not in lws's per-session
        // buffer, so no per-session allocation is needed here.
        { ServerProtocol::WsProtocolName, wsCallback, 0, 4096 },
        LWS_PROTOCOL_LIST_TERM
    };

    struct lws_context_creation_info info{};
    info.port = port;
    // lws documents `iface` as an interface name ("eth2"), not an IP
    // literal -- "lo" is the Linux loopback interface, which is what
    // actually restricts the vhost to 127.0.0.1/::1 here.
    info.iface = "lo";
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (context == nullptr) {
        char msg[128];
        snprintf(msg, sizeof(msg), "ERR bind failed on 127.0.0.1:%d (port already in use?)\n", port);
        writeStatus(pipeFd, msg);
        _exit(1);
    }

    writeStatus(pipeFd, "OK\n");

    // lws_service()'s timeout_ms parameter is documented as ignored since
    // lws v3.2 -- it sleeps until lws itself has something to do (a new
    // connection, incoming client data, a close), not on a schedule. Without
    // this, a broadcast queued between service() calls (from the pipe-read
    // loop below) sits unflushed until some unrelated lws event happens to
    // wake it -- which is exactly the multi-second stalls this was causing.
    // lws_cancel_service() is the documented way to force it to wake up on
    // demand; this thread just does that on a fixed tick for the process's
    // lifetime (it's killed by _exit() below, so it needs no shutdown path).
    std::thread([context]() {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            lws_cancel_service(context);
        }
    }).detach();

    fcntl(pipeFd, F_SETFL, fcntl(pipeFd, F_GETFL, 0) | O_NONBLOCK);
    std::string pendingLine;

    for (;;) {
        lws_service(context, 50);

        char buf[4096];
        ssize_t n = read(pipeFd, buf, sizeof(buf));
        if (n == 0) {
            break; // parent exited; shut down.
        } else if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                break;
        } else {
            pendingLine.append(buf, n);
            size_t nl;
            while ((nl = pendingLine.find('\n')) != std::string::npos) {
                std::string line = pendingLine.substr(0, nl);
                pendingLine.erase(0, nl + 1);
                // Line formats from the parent: "S\t<path>\t<ts>\t<value>"
                // (sample), "R\t<path>" (a favorite was removed), or
                // "O\t<path>,<path>,..." (favorites were reordered).
                size_t t0 = line.find('\t');
                if (t0 == std::string::npos)
                    continue;
                std::string cmd = line.substr(0, t0);
                std::string rest = line.substr(t0 + 1);

                if (cmd == "S") {
                    size_t t1 = rest.find('\t');
                    size_t t2 = t1 == std::string::npos ? std::string::npos : rest.find('\t', t1 + 1);
                    if (t1 == std::string::npos || t2 == std::string::npos)
                        continue;
                    std::string path = rest.substr(0, t1);
                    int64_t ts = strtoll(rest.substr(t1 + 1, t2 - t1 - 1).c_str(), nullptr, 10);
                    double value = strtod(rest.substr(t2 + 1).c_str(), nullptr);

                    auto &ring = ringFor(rings, path);
                    ring.push_back({ts, value});
                    if (ring.size() > RingSize)
                        ring.pop_front();

                    std::string update = "{\"type\":\"update\",\"path\":\"" + path + "\",\"ts\":" +
                                          std::to_string(ts) + ",\"value\":" + std::to_string(value) + "}";
                    broadcastToAllSessions(update);
                } else if (cmd == "R") {
                    const std::string &path = rest;
                    rings.erase(std::remove_if(rings.begin(), rings.end(),
                                                [&](const auto &entry) { return entry.first == path; }),
                                rings.end());
                    broadcastToAllSessions("{\"type\":\"remove\",\"path\":\"" + path + "\"}");
                } else if (cmd == "O") {
                    // rest is a comma-separated, favorites-ordered list of paths (empty if
                    // there are no favorites left). Re-sort `rings` to match so a client that
                    // (re)connects later gets the right order in its backfill too.
                    std::vector<std::string> order;
                    size_t pos = 0;
                    while (pos <= rest.size()) {
                        size_t comma = rest.find(',', pos);
                        std::string path = rest.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                        if (!path.empty())
                            order.push_back(path);
                        if (comma == std::string::npos)
                            break;
                        pos = comma + 1;
                    }

                    RingMap reordered;
                    reordered.reserve(rings.size());
                    for (const auto &path : order) {
                        auto it = std::find_if(rings.begin(), rings.end(),
                                                [&](const auto &entry) { return entry.first == path; });
                        if (it != rings.end()) {
                            reordered.push_back(std::move(*it));
                            rings.erase(it);
                        }
                    }
                    // Anything left wasn't in the order list (e.g. a non-numeric favorite,
                    // which never gets an "S" line and so has no ring here anyway) -- keep
                    // it at the tail rather than dropping it.
                    for (auto &entry : rings)
                        reordered.push_back(std::move(entry));
                    rings = std::move(reordered);

                    broadcastToAllSessions(buildOrderJson(order));
                }
            }
        }
    }

    lws_context_destroy(context);
    _exit(0);
}
