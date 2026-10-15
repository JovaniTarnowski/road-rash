#pragma once
#include "race.hpp"
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace rr::golden {
// Diagnostic frame-0 hydration only. The caller supplies the solo gate.
// A declaration makes missing/ambiguous state fatal; older demo captures
// retain their lateral-only fallback. Validate the complete input before
// mutating any rider, so partial captures cannot leave a half-seeded race.
inline void apply_frozen_state(const std::map<std::string, int64_t>& header,
                               std::vector<rr::Rider>& riders, bool demo) {
    const auto marker = header.find("probe.frozen_words");
    const bool declared = marker != header.end();
    if (declared && marker->second != 1)
        throw std::runtime_error("unsupported probe.frozen_words declaration");
    if (!declared && !demo) return;
    struct Node { std::string key; int64_t address, position, type; };
    std::map<int64_t, Node> by_address, bikes;
    std::map<int64_t, int32_t> old_bike_lat, old_half_lat;
    auto required = [&](const std::string& key) -> int64_t {
        auto it = header.find(key);
        if (it == header.end()) throw std::runtime_error("frozen state missing " + key);
        return it->second;
    };
    auto word = [&](const std::string& key) -> int32_t {
        const int64_t v = required(key);
        if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max())
            throw std::runtime_error("frozen state i32 out of range: " + key);
        return int32_t(v);
    };
    for (const auto& [key, address] : header) {
        if (key.rfind("nodeaddr.", 0) != 0 || key.find('.', 9) != std::string::npos) continue;
        const auto p = header.find(key + ".position"), t = header.find(key + ".type");
        if (p == header.end() || t == header.end()) continue;
        const Node n{key, address, p->second, t->second};
        if (!by_address.emplace(address, n).second && declared)
            throw std::runtime_error("duplicate frozen node address");
        if (n.type == 4 && !bikes.emplace(n.position, n).second && declared)
            throw std::runtime_error("duplicate frozen bike position " + std::to_string(n.position));
        const auto lat = header.find(key + ".lateral");
        if (!declared && lat != header.end()) {
            if (n.type == 4) old_bike_lat[n.position] = int32_t(lat->second);
            if (n.type == 8) old_half_lat[n.position] = int32_t(lat->second);
        }
    }
    if (!declared && old_bike_lat.empty()) return;
    static constexpr int offsets[] = {0x30,0x34,0x38,0x3c,0x40,0x108,0x10c,0x110,0x2f4};
    static constexpr const char* suffixes[] = {"30","34","38","3c","40","108","10c","110","2f4"};
    struct State {
        rr::Rider* rider;
        int32_t lateral, half_lateral;
        std::array<int32_t, 9> words{};
        uint8_t flags = 0;
        bool sched = false;
        int32_t sched_last = 0, sched_next = 0, sched_interval = 1, sched_dt = 1;
    };
    std::vector<State> states;
    std::set<int32_t> native_positions;
    std::set<int64_t> used_halves;
    for (auto& r : riders) {
        if (!r.frozen) continue;
        const int32_t position = r.bike.i32(0x1c);
        State state{&r, 0, 0};
        if (declared) {
            if (!native_positions.insert(position).second)
                throw std::runtime_error("duplicate native frozen position " + std::to_string(position));
            const auto bike = bikes.find(position);
            if (bike == bikes.end()) throw std::runtime_error("frozen bike missing at " + std::to_string(position));
            const std::string& key = bike->second.key;
            const int64_t half_address = required(key + ".half");
            const auto half = by_address.find(half_address);
            if (!half_address || half == by_address.end() || half->second.type != 8 || half->second.position != position)
                throw std::runtime_error("invalid linked type8 frozen half at " + std::to_string(position));
            if (!used_halves.insert(half_address).second)
                throw std::runtime_error("duplicate linked frozen half");
            state.lateral = word(key + ".lateral");
            state.half_lateral = word(half->second.key + ".lateral");
            for (size_t i = 0; i < state.words.size(); ++i)
                state.words[i] = word(key + ".bike_" + suffixes[i]);
            const int64_t flags = required(key + ".bike_c0");
            if (flags < 0 || flags > 255) throw std::runtime_error("frozen state byte out of range: " + key + ".bike_c0");
            state.flags = uint8_t(flags);
            // Scheduler metadata (0x00437e60): per-tick sched.<addr>.* keys
            // share frame 0 with the node dump. Present in sched900-era
            // captures; legacy captures keep the build dt 1 (sched false).
            auto sched = [&](const char* field) -> const int64_t* {
                char skey[64];
                std::snprintf(skey, sizeof skey, "sched.%x.%s",
                              uint32_t(bike->second.address), field);
                auto it = header.find(skey);
                return it == header.end() ? nullptr : &it->second;
            };
            const int64_t* s_last = sched("last");
            const int64_t* s_next = sched("next");
            const int64_t* s_interval = sched("interval");
            const int64_t* s_dt = sched("dt");
            if (s_last || s_next || s_interval || s_dt) {
                if (!s_last || !s_next || !s_interval || !s_dt)
                    throw std::runtime_error("frozen state partial sched keys at " + std::to_string(position));
                for (const int64_t* v : {s_last, s_next, s_interval, s_dt})
                    if (*v < std::numeric_limits<int32_t>::min() || *v > std::numeric_limits<int32_t>::max())
                        throw std::runtime_error("frozen state sched out of range at " + std::to_string(position));
                if (*s_interval < 1)
                    throw std::runtime_error("frozen state non-positive interval at " + std::to_string(position));
                state.sched = true;
                state.sched_last = int32_t(*s_last);
                state.sched_next = int32_t(*s_next);
                state.sched_interval = int32_t(*s_interval);
                state.sched_dt = int32_t(*s_dt);
            }
        } else {
            const auto bl = old_bike_lat.find(position), hl = old_half_lat.find(position);
            if (bl == old_bike_lat.end() || hl == old_half_lat.end())
                throw std::runtime_error("frozen rider at " + std::to_string(position) + " missing from the node dump");
            state.lateral = bl->second; state.half_lateral = hl->second;
        }
        states.push_back(state);
    }
    for (const auto& state : states) {
        auto& r = *state.rider;
        r.bike.i32(0x28) = state.lateral;
        if (declared) {
            for (size_t i = 0; i < state.words.size(); ++i) r.bike.i32(offsets[i]) = state.words[i];
            r.bike.u8(0xc0) = state.flags;
            if (state.sched) {
                r.sched_last = state.sched_last;
                r.sched_next = state.sched_next;
                r.sched_interval = state.sched_interval;
                r.sched_live = true;
                r.bike.i32(0x18) = state.sched_dt;
            }
        } else {
            for (int k = 0; k < 5; ++k) r.bike.i32(0x30 + 4*k) = r.bike.i32(0x1c + 4*k);
        }
        r.half_detached = true;
        r.half_lateral = state.half_lateral;
    }
}
}
