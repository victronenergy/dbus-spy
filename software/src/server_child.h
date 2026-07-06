#ifndef SERVER_CHILD_H
#define SERVER_CHILD_H

// Runs in the forked, privilege-dropped child process: drops to an
// unprivileged account, then serves the WebSocket data feed on
// 127.0.0.1:<port> (software/graph.html, one possible consumer, is not
// served here). Reads tagged lines from pipeFd -- "S\t<path>\t<ts>\t<value>"
// for a sample, "R\t<path>" when a favorite is removed -- and relays each to
// connected clients immediately. Before that loop starts it writes one
// status line back on the same fd ("OK\n" or "ERR <reason>\n"), so the
// parent learns the bind result without a second IPC channel. Never falls
// back to running as root on failure.
[[noreturn]] void runServerChild(int pipeFd, int port);

#endif
