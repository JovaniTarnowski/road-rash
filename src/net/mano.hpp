// Plan 34 Task 7: the lobby room client. One RoomClient owns the WebSocket to
// rr-netd for the life of a room visit: Hello/Welcome, create/join/list/leave,
// character picks, course votes, chat and the Start/Official race envelope.
// Race inputs and sealed frames stay in net::Client (Task 6); this class never
// simulates. All network I/O is main-thread polling: update() drains the
// channel, answers Ping and refreshes the cached state. Nothing here reads
// game data or the renderer, so headless tests drive it against a real Server.
#pragma once
#include "net/channel.hpp"
#include "net/protocol.hpp"
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rr::net {

class RoomClient {
  public:
    using Factory = std::function<std::unique_ptr<Channel>()>;

    enum class Conn { idle, connecting, open, failed };
    struct ChatLine {
        std::string alias;
        std::string text;
        int32_t tick = -1; // sealed race tick, or -1 in the lobby
    };

    // hello carries the compatibility gate (protocol, fingerprint, digest),
    // alias and, when reconnecting, the seat token from the previous Welcome.
    RoomClient(Factory factory, Hello hello);
    RoomClient(const RoomClient&) = delete;
    RoomClient& operator=(const RoomClient&) = delete;
    // Movable: no observer captures this client's address (unlike net::Client).
    RoomClient(RoomClient&&) = default;
    RoomClient& operator=(RoomClient&&) = default;

    // Lobby actions. Each queues one message; silently dropped while idle.
    void create_room(const RoomOptions& options);
    void join_room_text(const std::string& text, bool spectate = false); // code or invite link
    void request_list();
    void leave();
    void pick_character(int character); // 1..8
    void vote_course(int course);       // 1..5, original course order
    void set_ready(bool ready);
    void send_chat_slot(int slot, const std::string& text); // 0..7 = F1-F8
    void send_chat_text(const std::string& text);
    void reconnect(); // drop the channel and reconnect with the seat token

    void update(uint64_t now_ms); // poll the channel once per frame

    Conn conn() const { return conn_; }
    const std::string& error() const { return error_; } // reject/failed text, empty when fine
    bool welcomed() const { return welcomed_; }
    bool in_room() const { return room_.has_value(); }
    const std::optional<RoomState>& room() const { return room_; }
    int my_seat() const { return room_ ? int(room_->your_seat) : -1; }
    bool is_host() const;
    bool is_spectator() const;
    const std::optional<SeatToken>& seat_token() const { return seat_token_; }
    const std::vector<RoomListEntry>& rooms() const { return rooms_; }
    int vote_count() const; // seats with a course vote, for "x/y players have chosen"
    int occupied_count() const;

    std::optional<Start> take_start();       // pending Start, cleared on take
    std::optional<Official> take_official(); // pending Official, cleared on take
    std::deque<ChatLine> take_chat();        // queued chat relays, cleared on take
    bool has_pending_race() const { return start_.has_value(); }

    // Race handover. The lobby owns the reliable channel; when a Start arrives
    // the caller lends it to net::Client for the race (release_channel), and
    // takes it back afterwards (resume). While detached, update() only caches
    // race-bound state (Start/Official/chat) from inject(). inject() feeds one
    // lobby message drained from the race client's event queue back into the
    // roster/chat/room state, so nothing the server sent mid-race is lost.
    std::unique_ptr<Channel> release_channel();
    void resume(std::unique_ptr<Channel> channel);
    void inject(const Message& m, uint64_t now_ms);
    bool detached() const { return detached_; }

    // The local seat's race report. Production races report through
    // net::Client's automatic results; this covers the lobby-driven path and
    // lets the rematch flow be tested without a simulation.
    void send_result(int32_t tick, Outcome outcome, int place, uint64_t hash);

    // Roster announcement lines for the lobby ("Let's welcome %s.",
    // "%s is playing as %s.", "%s has chosen %s.", "%s has left the game.").
    // The frontend renders them with the original strings 402-408/420.
    struct RosterEvent {
        enum class Kind { joined, character, vote, left } kind = Kind::joined;
        std::string alias;
        int value = 0; // character or course
    };
    std::deque<RosterEvent> take_roster();

  private:
    void ensure_channel();
    void send(const Message& m);
    void on_message(const Message& m, uint64_t now_ms);
    void track_roster(const RoomState& next);

    Factory factory_;
    Hello hello_;
    std::unique_ptr<Channel> channel_;
    bool hello_sent_ = false, welcomed_ = false, detached_ = false, active_ = true;
    std::deque<Message> pending_;
    uint64_t connecting_at_ = 0;
    Conn conn_ = Conn::idle;
    std::string error_;
    std::optional<RoomState> room_, previous_room_;
    std::optional<SeatToken> seat_token_;
    std::vector<RoomListEntry> rooms_;
    std::optional<Start> start_;
    std::optional<Official> official_pending_;
    std::deque<ChatLine> chat_;
    std::deque<RosterEvent> roster_;
    uint64_t next_ping_ms_ = 0;
    uint32_t ping_seq_ = 0;
};

} // namespace rr::net
