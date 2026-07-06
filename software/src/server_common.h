#ifndef SERVER_COMMON_H
#define SERVER_COMMON_H

namespace ServerProtocol {

constexpr int DefaultPort = 57813;
constexpr const char *WsProtocolName = "dbus-spy";
// dbus-spy itself typically runs as root on Venus GX devices, so the forked
// server child must never process untrusted network input with root privileges.
constexpr const char *DropPrivilegesUser = "nobody";

} // namespace ServerProtocol

#endif
