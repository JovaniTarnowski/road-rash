// Plan 34 Task 4: libdatachannel WebSocket channel, client and server side.
#pragma once
#include "net/channel.hpp"

namespace rtc {
class WebSocket;
}

namespace rr::net {

// Wraps a WebSocket (from rtc::WebSocketServer or a client) as a Channel.
// Incoming payloads beyond `max_queued` unread messages fail the channel
// rather than grow without bound.
std::unique_ptr<Channel> make_ws_channel(std::shared_ptr<rtc::WebSocket> ws, size_t max_queued = 4096);

} // namespace rr::net
