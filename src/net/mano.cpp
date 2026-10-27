// Plan 34 Task 7: the lobby room client (see mano.hpp).
#include "net/mano.hpp"
#include "net/compat.hpp"

#include <algorithm>

namespace rr::net {

RoomClient::RoomClient(Factory factory, Hello hello) : factory_(std::move(factory)), hello_(hello) {}

void RoomClient::ensure_channel() {
    active_ = true;
    if (channel_ || conn_ == Conn::failed) return;
    channel_ = factory_();
    if (channel_) {
        conn_ = Conn::connecting;
        hello_sent_ = false;
    } else {
        conn_ = Conn::failed;
        error_ = "Could not reach the server. Check the network and retry.";
    }
}

void RoomClient::send(const Message& m) {
    ensure_channel();
    if (!channel_ || conn_ == Conn::failed) return;
    if (!welcomed_ || channel_->status() != ChannelStatus::open) {
        if (pending_.size() < 32)
            pending_.push_back(m);
        else {
            conn_ = Conn::failed;
            error_ = "Too many pending requests.";
        }
    } else
        channel_->send(encode(m));
}

void RoomClient::create_room(const RoomOptions& options) {
    error_.clear();
    send(RoomCreate{options});
}

void RoomClient::join_room_text(const std::string& text, bool spectate) {
    const auto code = parse_room_code(text);
    if (!code) {
        error_ = "No room with that code. Check the code or link.";
        return;
    }
    error_.clear();
    send(RoomJoin{*code, spectate});
}

void RoomClient::request_list() {
    send(RoomList{});
}

void RoomClient::leave() {
    if (in_room()) send(RoomLeave{});
    if (channel_) channel_->close();
    channel_.reset();
    pending_.clear();
    seat_token_.reset();
    hello_.seat_token.reset();
    conn_ = Conn::idle;
    active_ = false;
    welcomed_ = false;
    hello_sent_ = false;
    connecting_at_ = 0;
    room_.reset();
    previous_room_.reset();
    start_.reset();
}

void RoomClient::pick_character(int character) {
    if (character < 1 || character > 8 || !in_room()) return;
    send(PickCharacter{uint8_t(character)});
}

void RoomClient::vote_course(int course) {
    if (course < 1 || course > 5 || !in_room()) return;
    send(VoteCourse{uint8_t(course)});
    send(Ready{true});
}

void RoomClient::set_ready(bool ready) {
    if (!in_room()) return;
    send(Ready{ready});
}

void RoomClient::send_chat_slot(int slot, const std::string& text) {
    if (!in_room() || slot < 0 || slot > 7 || text.empty()) return;
    send(Chat{uint8_t(slot), text.substr(0, max_chat_text)});
}

void RoomClient::send_chat_text(const std::string& text) {
    if (!in_room() || text.empty()) return;
    send(Chat{0xff, text.substr(0, max_chat_text)});
}

void RoomClient::reconnect() {
    channel_.reset();
    pending_.clear();
    connecting_at_ = 0;
    active_ = true;
    hello_sent_ = false;
    welcomed_ = false;
    conn_ = Conn::idle;
    error_.clear();
    start_.reset();
}

bool RoomClient::is_host() const {
    return room_ && !is_spectator() && room_->host_seat == room_->your_seat;
}

bool RoomClient::is_spectator() const {
    return room_ && room_->your_seat == 0xff;
}

int RoomClient::vote_count() const {
    if (!room_) return 0;
    int n = 0;
    for (const SeatInfo& s : room_->seats)
        if (s.occupied && s.vote) ++n;
    return n;
}

int RoomClient::occupied_count() const {
    if (!room_) return 0;
    int n = 0;
    for (const SeatInfo& s : room_->seats)
        if (s.occupied) ++n;
    return n;
}

std::optional<Start> RoomClient::take_start() {
    auto out = start_;
    start_.reset();
    return out;
}

std::optional<Official> RoomClient::take_official() {
    auto out = official_pending_;
    official_pending_.reset();
    return out;
}

std::deque<RoomClient::ChatLine> RoomClient::take_chat() {
    std::deque<ChatLine> out;
    out.swap(chat_);
    return out;
}

std::deque<RoomClient::RosterEvent> RoomClient::take_roster() {
    std::deque<RosterEvent> out;
    out.swap(roster_);
    return out;
}

void RoomClient::track_roster(const RoomState& next) {
    // Diff against the previous RoomState to raise announcements. The first
    // RoomState only establishes the baseline (joining is announced by the
    // server's own arrival state once a *second* state shows the newcomer).
    if (!previous_room_) {
        previous_room_ = next;
        return;
    }
    const RoomState& prev = *previous_room_;
    for (int s = 0; s < max_seats; ++s) {
        const SeatInfo& a = prev.seats[size_t(s)];
        const SeatInfo& b = next.seats[size_t(s)];
        if (!a.occupied && b.occupied)
            roster_.push_back({RosterEvent::Kind::joined, b.alias, 0});
        else if (a.occupied && !b.occupied && !a.alias.empty())
            roster_.push_back({RosterEvent::Kind::left, a.alias, 0});
        else if (b.occupied && a.character != b.character && b.character)
            roster_.push_back({RosterEvent::Kind::character, b.alias, b.character});
        else if (b.occupied && a.vote != b.vote && b.vote)
            roster_.push_back({RosterEvent::Kind::vote, b.alias, b.vote});
    }
    previous_room_ = next;
    while (roster_.size() > 32) roster_.pop_front();
}

void RoomClient::on_message(const Message& m, uint64_t now_ms) {
    if (const auto* w = std::get_if<Welcome>(&m)) {
        welcomed_ = true;
        if (conn_ == Conn::connecting) conn_ = Conn::open;
        if (!w->room_code.empty()) {
            // (Re)took a seat: keep the token for the life of the room.
            seat_token_ = w->seat_token;
            hello_.seat_token = w->seat_token;
        } else if (w->seat_token != SeatToken{}) {
            seat_token_ = w->seat_token;
        }
        return;
    }
    if (const auto* r = std::get_if<Reject>(&m)) {
        // A rejection ends this attempt; the frontend shows error_ and offers
        // Leave/retry. A mid-room rejection never implictly ejects the roster:
        // RoomLeave clears it explicitly.
        error_ = reject_text(r->reason);
        if (!welcomed_ || !in_room()) conn_ = Conn::failed;
        return;
    }
    if (const auto* l = std::get_if<RoomListReply>(&m)) {
        rooms_ = l->rooms;
        return;
    }
    if (const auto* st = std::get_if<RoomState>(&m)) {
        room_ = *st;
        track_roster(*st);
        return;
    }
    if (const auto* c = std::get_if<ChatRelay>(&m)) {
        std::string alias = "Racer";
        if (room_ && c->seat < max_seats) {
            const std::string& a = room_->seats[c->seat].alias;
            if (!a.empty()) alias = a;
        }
        chat_.push_back({alias, c->text, c->tick});
        while (chat_.size() > 64) chat_.pop_front();
        return;
    }
    if (const auto* s = std::get_if<Start>(&m)) {
        // Freeze lobby polling immediately: following Frames/LogChunk belong
        // to the race, even if the UI cannot hand over until its next frame.
        detached_ = true;
        start_ = *s;
        return;
    }
    if (const auto* o = std::get_if<Official>(&m)) {
        official_pending_ = *o;
        return;
    }
    if (const auto* p = std::get_if<Ping>(&m)) {
        send(Pong{p->t, 0, uint32_t(now_ms)});
        return;
    }
    // Race frames, hashes and results flow through net::Client during a race;
    // lobby actions otherwise ignored here: Pong, LogChunk, Rtc*.
}

std::unique_ptr<Channel> RoomClient::release_channel() {
    detached_ = true;
    return std::move(channel_);
}

void RoomClient::resume(std::unique_ptr<Channel> channel) {
    channel_ = std::move(channel);
    // A live channel already hello'd; a fresh one must say Hello (with the
    // seat token, so the server re-takes the seat inside the window).
    hello_sent_ = channel_ != nullptr;
    detached_ = false;
    if (channel_ && conn_ == Conn::failed) conn_ = Conn::connecting;
}

void RoomClient::inject(const Message& m, uint64_t now_ms) {
    on_message(m, now_ms);
}

void RoomClient::send_result(int32_t tick, Outcome outcome, int place, uint64_t hash) {
    if (!in_room() || is_spectator()) return;
    send(Result{tick, outcome, uint8_t(std::clamp(place, 0, 15)), hash});
}

void RoomClient::update(uint64_t now_ms) {
    if (detached_) return; // the race client owns the channel; inject() caches state
    if (!active_) return;
    ensure_channel();
    if (!channel_) return;
    if (!connecting_at_) connecting_at_ = now_ms;
    if (!welcomed_ && now_ms - connecting_at_ > 10000) {
        conn_ = Conn::failed;
        error_ = reject_text(RejectReason::timeout);
        return;
    }
    if (!hello_sent_ && channel_->status() == ChannelStatus::open) {
        hello_sent_ = true;
        channel_->send(encode(Message(hello_)));
    }
    const ChannelStatus st = channel_->status();
    if (st == ChannelStatus::failed) {
        if (conn_ != Conn::failed) {
            conn_ = Conn::failed;
            if (error_.empty()) error_ = "The connection failed. Check the network and retry.";
        }
        return;
    }
    if (st == ChannelStatus::closed) {
        // The server closed us (idle timeout, kick, full room without a
        // Reject). Stay on the roster until the user leaves; a reconnect
        // re-takes the seat inside the window.
        if (conn_ != Conn::failed && error_.empty())
            error_ = "The server closed the connection. Reconnect to take the seat back.";
        return;
    }
    for (int budget = 0; budget < 256; ++budget) {
        auto bytes = channel_->poll();
        if (!bytes) break;
        if (bytes->empty() || bytes->size() > max_message_bytes) continue;
        if (auto m = decode(*bytes)) {
            on_message(*m, now_ms);
            if (std::holds_alternative<Start>(*m)) break;
        }
    }
    if (welcomed_ && conn_ != Conn::failed)
        while (!pending_.empty()) {
            channel_->send(encode(pending_.front()));
            pending_.pop_front();
        }
    // Roster ping column stays fresh without user action.
    if (welcomed_ && now_ms >= next_ping_ms_) {
        next_ping_ms_ = now_ms + 5000;
        send(Ping{++ping_seq_});
    }
}

} // namespace rr::net
