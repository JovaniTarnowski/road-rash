#include "actor_nodes.hpp"
#include <iostream>
int main() {
    try {
        rr::Assets assets(RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if (!assets.exists("DATA/GLOBAL.RSC")) {
            std::cout << "Imported actor nodes corpus absent\n";
            return 0;
        }
        const auto exe = assets.read("ROADRASH.EXE");
        rr::ActorNodes nodes(assets, exe, true);
        const auto depths = rr::read_depth_table(exe);
        size_t populated = 0, empty = 0, pixelless = 0;
        for (size_t bank = 0; bank < 4; ++bank)
            for (size_t f = 0; f < nodes.library().graphics[bank].frames.size(); ++f) {
                const auto* texture = nodes.texture(bank, uint32_t(f), 0);
                if (texture) {
                    const auto& graphic = nodes.library().graphics[bank].frames[f];
                    if (texture->mips.empty()) {
                        // 0x004436a0 null pixel pointer (e.g. empty SHADOW.DAT
                        // frames): the node is still emitted, pixels contribute
                        // nothing. Bounds check does not apply; stability does.
                        ++pixelless;
                    } else {
                        ++populated;
                        if (texture->bounds[2] < graphic.width || texture->bounds[3] < graphic.height)
                            throw std::runtime_error("Actor page smaller than RRAN logical dimensions");
                    }
                    if (nodes.texture(bank, uint32_t(f), 0) != texture)
                        throw std::runtime_error("Actor texture ownership unstable");
                } else
                    ++empty;
            }
        const auto* stable = nodes.texture(0, 3, 0);
        for (int colour = 1; colour < 15; ++colour) nodes.texture(0, 3, colour);
        if (stable != nodes.texture(0, 3, 0))
            throw std::runtime_error("Remap insertion invalidated actor texture");
        size_t drawn = 0;
        for (const auto& sequence : nodes.library().playback[0])
            if (sequence)
                for (const auto& descriptor : *sequence) {
                    rr::ActorOrientation orientation;
                    orientation.depth = 512;
                    rr::ActorProjection projection;
                    projection.depth = 512;
                    drawn += nodes.rider(descriptor, 0, 0, orientation, projection, {}, depths).size();
                }
        if (!drawn || !pixelless || !populated)
            throw std::runtime_error("Imported actor corpus not exercised");
        std::cout << populated << " owned actor pages, " << empty << " missing frames, " << pixelless
                  << " pixel-less frames, " << drawn << " descriptor nodes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
