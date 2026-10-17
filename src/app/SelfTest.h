#pragma once

#include <QString>

namespace p2p::app {

/// The Worker URL this build was configured with (P2P_API_BASE), possibly empty.
QString compiledApiBase();

/// Automated end-to-end check, run with `ModernP2P.exe --selftest`.
///
/// Starts two complete "devices" inside one process and exercises the real
/// network path: PeerJS signalling and WebRTC for the transfer and, when a
/// Worker URL is available, the Cloudflare Worker for code claim, lookup,
/// request, accept and notifications. Progress is written to `outFile`
/// (lines start with OK or FAIL). Returns 0 when everything passed.
int runSelfTest(const QString &outFile, const QString &apiBaseOverride);

} // namespace p2p::app
