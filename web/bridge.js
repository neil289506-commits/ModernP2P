// Hosts the PeerJS peer inside a hidden Qt WebEngine page. All file I/O and
// protocol logic lives in C++; this file only moves strings between Qt and
// PeerJS data connections.
(function () {
  "use strict";
  let host = null;
  let peer = null;
  let nextId = 1;
  const conns = {};
  const MAX_BUFFERED = 1 << 20; // 1 MiB

  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  function register(conn, cid) {
    conns[cid] = conn;
    conn.on("open", () => host.jsConnOpen(cid));
    conn.on("data", (d) => {
      if (typeof d === "string") host.jsMessage(cid, d);
    });
    conn.on("close", () => {
      delete conns[cid];
      host.jsConnClosed(cid);
    });
    conn.on("error", (e) => host.jsConnError(cid, String(e && e.message ? e.message : e)));
  }

  async function doSend(cid, msg, seq) {
    const conn = conns[cid];
    if (!conn) return;
    while (conn.dataChannel && conn.dataChannel.bufferedAmount > MAX_BUFFERED) await sleep(10);
    try {
      conn.send(msg);
    } catch (e) {
      host.jsConnError(cid, String(e));
      return;
    }
    host.jsSent(cid, seq);
  }

  window.bridge = {
    start(peerId) {
      if (peer && !peer.destroyed) peer.destroy();
      peer = new Peer(peerId, {
        debug: 0,
        config: { iceServers: [{ urls: "stun:stun.l.google.com:19302" }] },
      });
      peer.on("open", (id) => host.jsPeerOpen(id));
      peer.on("disconnected", () => host.jsPeerDisconnected());
      peer.on("error", (e) => host.jsPeerError(String(e.type) + ": " + String(e.message)));
      peer.on("connection", (conn) => {
        const cid = "i" + nextId++;
        register(conn, cid);
        host.jsIncoming(cid, conn.peer, JSON.stringify(conn.metadata || {}));
      });
    },
    reconnect() {
      if (peer && peer.disconnected && !peer.destroyed) peer.reconnect();
    },
    connectTo(cid, remotePeerId, metadataJson) {
      const conn = peer.connect(remotePeerId, {
        reliable: true,
        serialization: "raw",
        metadata: JSON.parse(metadataJson),
      });
      register(conn, cid);
    },
    // Sends are chained per connection so ordering is preserved while we wait
    // for the data channel buffer to drain.
    send(cid, msg, seq) {
      const conn = conns[cid];
      if (!conn) return;
      conn._chain = (conn._chain || Promise.resolve()).then(() => doSend(cid, msg, seq));
    },
    close(cid) {
      const conn = conns[cid];
      if (conn) conn.close();
    },
  };

  new QWebChannel(qt.webChannelTransport, (channel) => {
    host = channel.objects.host;
    host.jsReady();
  });
})();
