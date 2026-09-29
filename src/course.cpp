#include "course.hpp"
#include <algorithm>
#include <set>

namespace rr {
namespace {
struct Reader {
    const std::vector<uint8_t>& b;
    size_t base, size;
    void range(size_t p,size_t n) const {
        if (p>size || n>size-p) throw std::runtime_error("Course payload range outside resource");
    }
    uint32_t u32(size_t p) const {
        range(p,4); p+=base;
        return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;
    }
    int64_t s32(size_t p) const { auto v=u32(p); return v<=INT32_MAX ? int64_t(v) : int64_t(v)-0x100000000LL; }
    int byte(size_t p) const { range(p,1); return b[base+p]; }
    int signed_byte(size_t p) const { int v=byte(p); return v<128?v:v-256; }
    void magic(size_t p,const char* tag) const {
        range(p,4);
        if (!std::equal(tag,tag+4,b.begin()+base+p)) throw std::runtime_error("Unexpected course chunk tag");
    }
    void chunk(size_t p,const char* tag,size_t header,size_t stride) const {
        magic(p,tag); range(p,header);
        uint32_t extent=u32(p+4), count=u32(p+8);
        if (extent<header || count>(extent-header)/stride) throw std::runtime_error("Invalid course chunk count/size");
        range(p,extent);
    }
};

// Forward walk of an interval table exactly as the per-sample iterators step it
// (0x00452920 lanes, 0x00452b70 sides): records {start, end, bytes..., Q16 slopes}.
// Values start at record 0's start bytes. Stepping to sample j: within a record,
// j < end adds the slopes and j == end loads the end bytes; past it, the next record
// is entered only when j equals its start, keeping the current values. The last
// record is the terminal marker that 0x00447c20 rewrites to the node length.
struct Field { size_t first, last, slope; };
template<class Emit>
void walk_intervals(const Reader& r,size_t chunk,const char* tag,size_t stride,
                    std::initializer_list<Field> fields,uint32_t count,Emit emit) {
    r.chunk(chunk,tag,16,stride);
    const uint32_t n=r.u32(chunk+8);
    if(!n) throw std::runtime_error("Empty interval table");
    auto record=[&](uint32_t k){ return chunk+16+stride*k; };
    for(uint32_t k=0;k<n;++k) if(r.u32(record(k))>r.u32(record(k)+4)) throw std::runtime_error("Inverted interval");
    uint32_t k=0;
    std::vector<int64_t> value;
    for(const auto& f:fields) value.push_back(int64_t(r.byte(record(0)+f.first))*65536);
    for(uint32_t j=0;j<count;++j) {
        if(j) {
            const uint32_t end=r.u32(record(k)+4);
            if(j<end) { size_t i=0; for(const auto& f:fields) value[i++]+=r.s32(record(k)+f.slope); }
            else if(j==end) { size_t i=0; for(const auto& f:fields) value[i++]=int64_t(r.byte(record(k)+f.last))*65536; }
            else if(k+1<n && r.u32(record(k+1))==j) ++k;
        }
        for(auto v:value) if(v<0 || v>255LL*65536+65535) throw std::runtime_error(std::string(tag)+" value outside byte range");
        emit(j,value,record(k));
    }
}
}

Course read_course(const std::vector<uint8_t>& bytes) {
    auto resources=read_resources(bytes);
    auto get=[&](const char* tag) {
        auto it=std::find_if(resources.begin(),resources.end(),[&](const Resource& r){return r.tag==tag && r.id==1;});
        if(it==resources.end()) throw std::runtime_error(std::string("Missing course resource ")+tag);
        return Reader{bytes,it->offset,it->size};
    };
    auto segments=get("SGS "); auto graph=get("NOD ");
    segments.chunk(0,"SGSR",16,52);
    graph.magic(0,"DONR"); graph.range(0,20);
    if (graph.u32(4)!=graph.size) throw std::runtime_error("Node resource size mismatch");
    Course course{graph.u32(16),{}, {}};
    for(size_t i=0;i<segments.u32(8);++i) {
        size_t row=16+i*52, path=segments.u32(row+4), sides=segments.u32(row+0x14), lanes=segments.u32(row+0x18);
        segments.chunk(path,"HTPR",16,2);
        const uint32_t count=segments.u32(path+8);
        CourseSection section;
        for(uint32_t j=0;j<count;++j)
            // No lane table: 0x004528f0 reports 0x200 (two lanes) each side.
            section.samples.push_back({segments.signed_byte(path+16+2*j),segments.signed_byte(path+17+2*j),500,500,512,512});
        // RLAN widths in lane units (0x00452820 / 0x00452920); 0x00454240 converts
        // Q8 lane units to 250-unit lanes.
        if(lanes) walk_intervals(segments,lanes,"NALR",20,{{8,10,12},{9,11,16}},count,
            [&](uint32_t j,const std::vector<int64_t>& q16,size_t) {
                for(int side=0;side<2;++side) {
                    const int64_t q8=q16[side]/256;
                    (side?section.samples[j].right:section.samples[j].left)=int32_t(q8*250/256);
                    (side?section.samples[j].right_q8:section.samples[j].left_q8)=int32_t(q8);
                }
            });
        // RSLD sides in world units (0x00452a00 / 0x00452b70, read back >>16 by 0x00452b00).
        if(sides) walk_intervals(segments,sides,"DLSR",36,{{8,10,20},{9,11,24},{12,14,28},{13,15,32}},count,
            [&](uint32_t j,const std::vector<int64_t>& q16,size_t record) {
                auto& sample=section.samples[j];
                sample.shoulder_left=int32_t(q16[0]>>16); sample.shoulder_right=int32_t(q16[1]>>16);
                sample.height_left=int32_t(q16[2]>>16); sample.height_right=int32_t(q16[3]>>16);
                sample.side_flags[0]=uint8_t(segments.byte(record+16)); sample.side_flags[1]=uint8_t(segments.byte(record+17));
            });
        course.sections.push_back(std::move(section));
    }
    // 0x00447b80 links +8/+12 for kind 0; +8/+12/+16 for kinds 1/2.
    std::vector<uint32_t> pending{course.root};
    while(!pending.empty()) {
        auto offset=pending.back(); pending.pop_back();
        if(course.nodes.count(offset)) continue;
        graph.range(offset,8);
        if(offset<20 || offset%4) throw std::runtime_error("Invalid course node offset");
        CourseNode node{offset,graph.u32(offset)};
        if(node.kind>4) throw std::runtime_error("Unknown course node kind "+std::to_string(node.kind)+" at "+std::to_string(offset));
        if(node.kind<3) {
            graph.range(offset,32);
            node.next=graph.u32(offset+8); node.previous=graph.u32(offset+12);
            node.link=graph.u32(offset+16); node.length=graph.u32(offset+20);
            pending.push_back(node.next); pending.push_back(node.previous);
            if(node.kind) pending.push_back(node.link);
            else if(node.link>=course.sections.size() || !node.length || node.length>course.sections[node.link].samples.size())
                throw std::runtime_error("Invalid section reference or node length");
        }
        course.nodes.emplace(offset,node);
        if(course.nodes.size()>graph.u32(8)) throw std::runtime_error("Node count exceeds header");
    }
    if(course.nodes.size()!=graph.u32(8)) throw std::runtime_error("Unreachable nodes or incorrect graph count");
    return course;
}

namespace {
// World-unit edge shift for a Q8 lane-width change: trunc(q8 * 250 / 256).
int32_t lane_shift(int32_t q8) { return int32_t(int64_t(q8)*250/256); }

const CourseNode& node_at(const Course& course,uint32_t offset) {
    auto found=course.nodes.find(offset);
    if(found==course.nodes.end()) throw std::runtime_error("Missing route node");
    return found->second;
}

// Samples of a branch from road node `offset` up to the next split/merge node.
std::vector<PathSample> branch_forward(const Course& course,uint32_t offset) {
    std::vector<PathSample> samples;
    for(size_t guard=0;guard<=course.nodes.size();++guard) {
        const auto& node=node_at(course,offset);
        if(node.kind!=0) return samples;
        const auto& section=course.sections.at(node.link);
        samples.insert(samples.end(),section.samples.begin(),section.samples.begin()+node.length);
        offset=node.next;
    }
    throw std::runtime_error("Cycle in course branch");
}

// Samples of a branch ending at road node `offset`, walking back to the previous split.
std::vector<PathSample> branch_backward(const Course& course,uint32_t offset) {
    std::vector<std::vector<PathSample>> parts;
    for(size_t guard=0;guard<=course.nodes.size();++guard) {
        const auto& node=node_at(course,offset);
        if(node.kind!=0) {
            std::vector<PathSample> samples;
            for(auto part=parts.rbegin();part!=parts.rend();++part) samples.insert(samples.end(),part->begin(),part->end());
            return samples;
        }
        const auto& section=course.sections.at(node.link);
        parts.emplace_back(section.samples.begin(),section.samples.begin()+node.length);
        offset=node.previous;
    }
    throw std::runtime_error("Cycle in course branch");
}

// 0x00453b60 walks 0x51 samples from each split/merge and rewrites node +0x14 to 0x5100.
constexpr uint32_t window=0x51;
}

std::vector<RouteSample> make_route(const Course& course,bool second_branch) {
    std::vector<RouteSample> route;
    std::set<uint32_t> visited;
    struct Split { size_t index; uint32_t other; };
    struct Merge { size_t index; uint8_t half; uint32_t other; };
    std::vector<Split> splits;
    std::vector<Merge> merges;
    uint32_t offset=course.root, arrived=0;
    while(true) {
        if(!visited.insert(offset).second) throw std::runtime_error("Cycle in forward course route");
        const auto& node=node_at(course,offset);
        if(node.kind>=3) break;
        if(node.kind==0) {
            const auto& section=course.sections.at(node.link);
            for(uint32_t i=0;i<node.length;++i) route.push_back({section.samples.at(i),offset,node.link,i});
            arrived=offset;
            offset=node.next;
        } else if(node.kind==1) {
            splits.push_back({route.size(),second_branch?node.next:node.previous});
            offset=second_branch?node.previous:node.next;
        } else {
            const uint8_t half=arrived==node.next?0:1;
            merges.push_back({route.size(),half,half?node.next:node.previous});
            offset=node.link;
        }
    }
    if(route.empty()) throw std::runtime_error("Empty course route");

    // Replay the lookahead traversal (0x00453030) along the route.
    auto merge_at=[&](size_t i){ for(auto& m:merges) if(m.index==i) return &m; return (Merge*)nullptr; };
    auto split_at=[&](size_t i){ for(auto& s:splits) if(s.index==i) return &s; return (Split*)nullptr; };
    bool dual=false, after_split=false;
    uint8_t primary=0;
    int32_t own_offset=0, other_offset=0, dx_sep=0, x_sep=0;
    std::vector<PathSample> other;
    size_t other_index=0;
    // Merge window states from the backward walk of 0x00453b60, indexed by distance to merge.
    struct Snapshot { size_t other_index; int32_t dx_sep, x_sep; };
    std::vector<Snapshot> snapshot;
    int32_t snapshot_offset=0;
    const Merge* pending=nullptr;
    for(size_t i=0;i<route.size();++i) {
        auto& sample=route[i];
        auto halves=[&](const PathSample& own,const PathSample& o) {
            return primary ? std::pair<const PathSample*,const PathSample*>{&o,&own}
                           : std::pair<const PathSample*,const PathSample*>{&own,&o};
        };
        if(const Merge* m=merge_at(i)) {
            // Merge reached: keep one road; the kept edge stays aligned.
            const auto& before=route[i-1].path;
            own_offset+=primary ? lane_shift(before.right_q8-sample.path.right_q8)
                                : lane_shift(sample.path.left_q8-before.left_q8);
            dual=false; after_split=false; pending=nullptr; (void)m;
            sample.event=RoadEvent::merged;
        } else if(const Split* s=split_at(i)) {
            other=branch_forward(course,s->other);
            if(other.empty()) throw std::runtime_error("Empty split branch");
            primary=second_branch?1:0;
            other_index=0;
            const auto& before=route[i-1].path;
            auto [h0,h1]=halves(sample.path,other[0]);
            const int32_t o0=own_offset+lane_shift(h0->left_q8-before.left_q8);
            const int32_t o1=own_offset+lane_shift(before.right_q8-h1->right_q8);
            own_offset=primary?o1:o0; other_offset=primary?o0:o1;
            dx_sep=h1->bend-h0->bend; x_sep=0;
            dual=true; after_split=true;
            sample.event=RoadEvent::split;
        } else if(dual) {
            // Primary crossing into a new road node, or past the window, drops the other road (0x00452fd0).
            if(after_split && (sample.node!=route[i-1].node || (sample.local<<8)>(window<<8))) {
                dual=false; after_split=false; primary=0;
            } else {
                if(other_index+1<other.size()) ++other_index;
                auto [h0,h1]=halves(sample.path,other[other_index]);
                x_sep+=dx_sep; dx_sep+=h1->bend-h0->bend;
            }
        } else {
            // 81 samples before a merge the traversal restores its precomputed dual snapshot.
            const auto& node=node_at(course,sample.node);
            const auto& next=node_at(course,node.next);
            if(next.kind==2 && node.length-sample.local==window) {
                pending=nullptr;
                for(auto& m:merges) if(m.index>i && (!pending || m.index<pending->index)) pending=&m;
                if(!pending || pending->index!=i+window) throw std::runtime_error("Merge window mismatch");
                primary=pending->half;
                other=branch_backward(course,pending->other);
                if(other.empty()) throw std::runtime_error("Empty merge branch");
                // Backward walk from the merge (0x00453b60 -> 0x00453030, negative steps).
                std::vector<const PathSample*> mine;
                for(size_t k=i;k<pending->index;++k) mine.push_back(&route[k].path);
                snapshot.assign(window,{});
                auto other_at=[&](size_t d){ return other.size()>d ? other.size()-1-d : 0; };
                auto d_at=[&](size_t d) {
                    auto [h0,h1]=halves(*mine[mine.size()-1-d],other[other_at(d)]);
                    return h1->bend-h0->bend;
                };
                int32_t s5=-d_at(0), s6=0;
                for(size_t d=0;d<window;++d) {
                    snapshot[d]={other_at(d),s5,s6};
                    s5-=d_at(d); s6-=s5;
                }
                // Offsets relative to the outgoing road, as the backward merge entry aligns them.
                const auto& out=route[pending->index].path;
                auto [h0,h1]=halves(*mine.back(),other.back());
                const int32_t o0=lane_shift(h0->left_q8-out.left_q8), o1=lane_shift(out.right_q8-h1->right_q8);
                snapshot_offset=primary ? o0-o1 : o1-o0;
                other_offset=own_offset+snapshot_offset;
                dual=true; after_split=false;
                sample.event=RoadEvent::merge_window;
            }
        }
        if(dual && pending && !after_split) {
            const auto& snap=snapshot[pending->index-1-i];
            other_index=snap.other_index; dx_sep=snap.dx_sep; x_sep=snap.x_sep;
        }
        sample.offset=own_offset;
        if(dual) {
            sample.dual=true; sample.half=primary; sample.other=other[other_index];
            sample.other_offset=other_offset; sample.dx_sep=dx_sep; sample.x_sep=x_sep;
        }
    }
    return route;
}
}
