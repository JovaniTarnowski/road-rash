#include "scene.hpp"
#include "golden.hpp"
#include "assets.hpp"
#include "terrain_panels.hpp"
#include "exe_tables.hpp"
#include <filesystem>
#include <iostream>
int main() {
    int failures=0;auto check=[&](bool ok,const char* msg){if(!ok){++failures;std::cerr<<msg<<'\n';}};
    rr::TerrainSlice hills;hills.type=4;
    for(int i=0;i<6;++i) hills.points[i]={i%2?500:-500,i*16};
    const auto projected=rr::project_hills(hills,0,0,{0,150},32768);
    check(projected.points[0][0]==-90*65536 && projected.points[1][0]==410*65536,"far hill x");
    check(projected.points[0][1]==165*65536 && projected.maximum_row==165*65536,"hill y and maximum row");
    hills.points[0][0]=-40000;hills.points[1][0]=40000;
    const auto near=rr::project_hills(hills,0,0,{0,150},65536);
    check(near.points[0][0]==-0x40000000 && near.points[1][0]==0x40000000,"near overflow sentinels");
    rr::DepthTable synthetic;synthetic.scale.assign(4352,65536);
    rr::HillStrip strip{{{{100,0},{200,0},{200,0},{100,0}}},
        {{{100*65536,90*65536},{200*65536,90*65536},{200*65536,100*65536},{100*65536,100*65536}}},1024,400*65536,1,false};
    check(rr::subdivide_hill(strip,{0,150},synthetic).size()==2,"far hill splits twice");
    strip.far_depth=465;check(rr::subdivide_hill(strip,{0,150},synthetic).size()==7,"near hill clips last segment below minimum depth");
    strip.far_depth=210;check(rr::subdivide_hill(strip,{0,150},synthetic).empty(),"reject too-near hill strip");
    strip.far_depth=1024;strip.occlusion_row=80*65536;
    check(rr::subdivide_hill(strip,{0,150},synthetic).empty(),"reject occluded hill strip");
    {
        // Terrain type 1 sides (0x0040dfd0 wall, 0x0040d030 cliff). Before
        // these were ported, mountain sections drew nothing beside the
        // shoulder and Pacific Highway showed the clear colour there.
        const int32_t q=65536;
        rr::CelTexture face,top,water;
        auto pick=[&](uint8_t frame,int32_t,int32_t){return frame?&top:&face;};
        auto pt=[&](int x,int y){return rr::RasterPoint{x*q,y*q};};
        auto same=[](const std::array<rr::RasterPoint,4>& a,const std::array<rr::RasterPoint,4>& b){
            for(size_t i=0;i<4;++i)if(a[i].x!=b[i].x || a[i].y!=b[i].y)return false;return true;};
        auto eq=[](rr::RasterPoint a,rr::RasterPoint b){return a.x==b.x && a.y==b.y;};
        rr::WallPanelInput wall;wall.side=1;wall.height=50;wall.near_lane_width=40*q;wall.far_lane_width=20*q;
        wall.near.type=1;wall.near.depth=300;wall.near.projected={pt(200,120),pt(220,120),pt(230,80),pt(230,80)};
        wall.far.type=1;wall.far.depth=556;wall.far.projected={pt(180,100),pt(190,100),pt(195,70),pt(195,70)};
        auto nodes=rr::wall_draw_nodes(wall,pick,&water);
        check(nodes.size()==3,"wall face plus two plateau tiles");
        if(nodes.size()==3) {
            const std::array<rr::RasterPoint,4> face_points={pt(195,70),pt(231,80),pt(221,121),pt(190,101)};
            check(nodes[0].texture==&face && same(nodes[0].view_points,face_points),"wall face from far top to far base, near edge one column out");
            const std::array<rr::RasterPoint,4> tile={pt(195,70),pt(275,70),pt(391,80),pt(231,80)};
            check(nodes[1].texture==&top && same(nodes[1].view_points,tile),"first plateau tile four lanes wide");
            check(nodes[2].view_points[0].x==275*q && nodes[2].view_points[3].x==391*q,"plateau tiles step until the view edge");
        }
        wall.near.depth=60;check(rr::wall_draw_nodes(wall,pick,&water).empty(),"no wall within 0x3d of the camera");
        // Hill neighbour: far top/base are its first point and shoulder.
        wall.near.depth=300;wall.far.type=4;
        nodes=rr::wall_draw_nodes(wall,pick,&water);
        check(!nodes.empty() && eq(nodes[0].view_points[0],pt(190,100)) && eq(nodes[0].view_points[3],pt(180,101)),"wall joins a hill at its shoulder");
        // Cliff down to the water, right side: water clamps to the view edge
        // unless within 256 columns of it.
        rr::WallPanelInput cliff;cliff.side=1;cliff.height=-100;
        cliff.near.type=1;cliff.near.depth=300;cliff.near.projected={pt(210,120),pt(220,120),pt(350,200),pt(350,200)};
        cliff.far.type=1;cliff.far.depth=556;cliff.far.projected={pt(225,100),pt(230,100),pt(300,150),pt(300,150)};
        nodes=rr::wall_draw_nodes(cliff,pick,&water);
        check(nodes.size()==2,"cliff face plus water");
        if(nodes.size()==2) {
            const std::array<rr::RasterPoint,4> face_points={rr::RasterPoint{230*q,100*q-0x6000},rr::RasterPoint{220*q,120*q-0x6000},pt(350,200),pt(300,150)};
            check(nodes[0].texture==&face && same(nodes[0].view_points,face_points),"cliff face from the road edge to the water");
            const std::array<rr::RasterPoint,4> sea={pt(350,200),pt(606,200),pt(556,150),pt(300,150)};
            check(nodes[1].texture==&water && same(nodes[1].view_points,sea),"right water strip");
        }
        cliff.near.projected[2]={20*q,200*q};cliff.far.projected[2]={10*q,150*q};
        nodes=rr::wall_draw_nodes(cliff,pick,&water);
        check(nodes.size()==2 && nodes[1].view_points[1].x==320*q && nodes[1].view_points[2].x==320*q,"water reaches the right view edge");
        // Left: 0x0040d2f2 emits water only when both feet are left of 0.
        cliff.side=0;cliff.near.projected={pt(110,120),pt(100,120),pt(-30,200),pt(-30,200)};
        cliff.far.projected={pt(95,100),pt(90,100),pt(-10,150),pt(-10,150)};
        nodes=rr::wall_draw_nodes(cliff,pick,&water);
        const std::array<rr::RasterPoint,4> left_sea={pt(-30,200),pt(-286,200),pt(-266,150),pt(-10,150)};
        check(nodes.size()==2 && same(nodes[1].view_points,left_sea),"left water strip off-screen");
        cliff.far.projected[2]={10*q,150*q};
        check(rr::wall_draw_nodes(cliff,pick,&water).size()==1,"no left water while a foot is on-screen");
        // Building neighbour: its facade fields at +0x70/+0x80 become the far
        // base/top (unreachable on the disc's routes, ported as original).
        wall.near.depth=300;wall.far.type=0;wall.far.projected[1]={-50*q,100*q};wall.far.projected[2]={-60*q,120*q};
        nodes=rr::wall_draw_nodes(wall,pick,&water);
        check(!nodes.empty() && eq(nodes[0].view_points[0],pt(-60,120)) && eq(nodes[0].view_points[3],pt(-50,101)),"wall reads a building neighbour's facade fields");
        // Tunnel sides: the mountain face runs to the +0xf4/+0xf8 points;
        // the open side adds the face up to them; inside with the camera
        // also in the tunnel neither is drawn.
        wall.far.type=1;wall.far.projected={pt(180,100),pt(190,100),pt(195,70),pt(195,70)};
        wall.near_tunnel=wall.far_tunnel=true;wall.near_tunnel_point=pt(300,40);wall.far_tunnel_point=pt(260,60);
        nodes=rr::wall_draw_nodes(wall,pick,&water);
        check(!nodes.empty() && eq(nodes[0].view_points[0],pt(195,70)) && eq(nodes[0].view_points[2],pt(301,41)) && eq(nodes[0].view_points[3],pt(260,61)),"tunnel mountain face ends at the tunnel point");
        wall.camera_in_tunnel=true;check(rr::wall_draw_nodes(wall,pick,&water).empty(),"no tunnel side while the camera is inside");
        cliff.side=1;cliff.near.projected={pt(210,120),pt(220,120),pt(350,200),pt(350,200)};cliff.far.projected={pt(225,100),pt(230,100),pt(300,150),pt(300,150)};
        cliff.near_tunnel=true;cliff.near_tunnel_point=pt(200,60);cliff.far_tunnel_point=pt(210,70);
        nodes=rr::wall_draw_nodes(cliff,pick,&water);
        check(nodes.size()==3 && eq(nodes[2].view_points[0],pt(210,70)) && eq(nodes[2].view_points[2],pt(220,120)),"open tunnel side face");
        check(rr::fixed_multiply(0x40000,0x4000)==0x10000 && rr::fixed_multiply(-0x40000,0x4000)==-0x10000,"0x0044f6b0 fixed multiply");
        // 0x0040dac0 arch: six panels, carried-over points, forced flips.
        rr::TunnelArchInput arch;arch.near_depth=300;arch.far_depth=556;arch.near_row=200*q;arch.backdrop=true;
        arch.near={pt(40,200),pt(40,100),pt(120,40),pt(200,40),pt(280,100),pt(280,200)};
        arch.far={pt(100,150),pt(100,110),pt(140,80),pt(180,80),pt(220,110),pt(220,150)};
        std::vector<uint8_t> kinds;
        auto arch_material=[&](uint8_t kind,int32_t,int32_t){kinds.push_back(kind);return &face;};
        nodes=rr::tunnel_arch_nodes(arch,arch_material);
        check(nodes.size()==7 && nodes[0].flat_index && *nodes[0].flat_index==0xfd && same(nodes[0].view_points,{pt(100,80),pt(220,80),pt(220,200),pt(100,200)}),"tunnel backdrop rectangle first");
        if(nodes.size()==7) {
            check(same(nodes[1].view_points,{pt(101,151),pt(101,110),pt(40,100),pt(40,201)}) && !nodes[1].flip_u && nodes[1].flip_v,"left tunnel wall panel");
            check(nodes[3].view_points[1].x==(319*q)>>1 && nodes[3].view_points[2].x==160*q,"roof halves meet at the midpoints");
            check(same(nodes[6].view_points,{rr::RasterPoint{219*q,151*q},rr::RasterPoint{219*q,109*q},rr::RasterPoint{280*q,99*q},rr::RasterPoint{280*q,201*q}}) && nodes[6].flip_u,"right tunnel wall panel");
            check(kinds==std::vector<uint8_t>({2,1,0,0,1,2}),"arch materials wall, haunch, roof");
        }
        nodes=rr::tunnel_arch_nodes(arch,[&](uint8_t kind,int32_t,int32_t){return kind==2?&face:nullptr;});
        check(nodes.size()==2,"a missing arch material ends the arch");
        arch.near_depth=0x3f;check(rr::tunnel_arch_nodes(arch,arch_material).empty(),"no arch within 0x3f");
        // 0x0040d420 portal: five face quads, five trim quads, two curbs.
        rr::TunnelPortalInput portal;portal.mountain=0;portal.row=200*q;portal.depth=300;
        portal.base={pt(20,200),pt(300,200)};portal.top={pt(0,20),pt(320,20)};portal.tunnel_point=pt(300,40);
        portal.arch={pt(40,210),pt(40,100),pt(120,40),pt(200,40),pt(280,100),pt(280,210)};
        portal.edge={pt(60,196),pt(260,196)};portal.shoulder_x={40*q,280*q};
        std::vector<uint8_t> parts;
        nodes=rr::tunnel_portal_nodes(portal,[&](uint8_t part,int32_t,int32_t){parts.push_back(part);return &face;});
        check(nodes.size()==12,"portal face, trim and curbs");
        if(nodes.size()==12) {
            check(eq(nodes[0].view_points[0],pt(18,200)) && eq(nodes[0].view_points[3],pt(15,200)),"portal face starts two columns left of the wall base");
            check(nodes[5].view_points[0].x==15*q-0x14000 && eq(nodes[5].view_points[3],rr::RasterPoint{40*q+0x14000,200*q}),"portal trim overlaps 1.25 columns");
            check(same(nodes[10].view_points,{pt(60,196),pt(40,196),pt(40,200),pt(60,200)}),"portal curb face");
        }
        check(parts==std::vector<uint8_t>({0,0,0,0,0,1,2}),"trim and curb materials chosen once");
    }
    const auto path=rr::golden::oracle_dir()+"/raster-fixtures.tsv";
    if(std::filesystem::exists(path)) {
        try {
            const auto golden=rr::golden::load(path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.empty()) throw std::runtime_error("Wrong scene oracle");
            const auto& frame=golden.frames.front();
            for(int c=0;c<96;++c) {
                const std::string prefix="hill.project."+std::to_string(c)+".";
                auto field=[&](const std::string& key) {
                    const auto i=frame.find(prefix+key);
                    if(i==frame.end() || i->second<INT32_MIN || i->second>INT32_MAX) throw std::runtime_error("Missing/invalid hill field: "+prefix+key);
                    return int32_t(i->second);
                };
                for(int i=0;i<6;++i) hills.points[i]={field("input."+std::to_string(i*2)),field("input."+std::to_string(i*2+1))};
                const auto actual=rr::project_hills(hills,field("world_x"),field("world_y"),{field("camera_x"),field("camera_height")},field("scale"),field("width"));
                for(int i=0;i<12;++i) check(actual.points[i/2][i%2]==field("output."+std::to_string(i)),"original hill point");
                check(actual.maximum_row==field("maximum_row"),"original hill maximum row");
            }
            std::cout<<"96 original hill projections compared\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    const auto strip_path=rr::golden::oracle_dir()+"/napa-hill-strips.tsv";
    if(std::filesystem::exists(strip_path)) {
        try {
            const auto golden=rr::golden::load(strip_path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.size()!=1)throw std::runtime_error("Wrong hill strip oracle");
            const auto& fields=golden.frames[0];
            rr::Assets assets((std::filesystem::path(RR_SOURCE_DIR).parent_path()/"cdrom/ROADRASH").string());
            const auto table=rr::read_depth_table(assets.read("ROADRASH.EXE"));
            for(int c=0;c<128;++c) {
                const std::string prefix="hill.strip."+std::to_string(c)+".";
                auto field=[&](const std::string& key) {const auto i=fields.find(prefix+key);if(i==fields.end() || i->second<INT32_MIN || i->second>INT32_MAX)throw std::runtime_error("Missing hill strip field: "+prefix+key);return int32_t(i->second);};
                rr::HillStrip input;input.side=uint8_t(field("input.0"));input.far_depth=field("input.1");input.occlusion_row=field("input.2");input.adjust_seam=field("seam")!=0;
                for(int i=0;i<4;++i){input.world[i]={field("input."+std::to_string(3+i*2)),field("input."+std::to_string(4+i*2))};input.projected[i]={field("projected."+std::to_string(i*2)),field("projected."+std::to_string(i*2+1))};}
                const auto actual=rr::subdivide_hill(input,{field("camera_x"),field("camera_height")},table,field("width"));
                check(int(actual.size())==field("count"),"original hill subdivision count");
                for(size_t n=0;n<actual.size() && int(n)<field("count");++n)for(int i=0;i<8;++i)
                    check((i%2?actual[n][i/2].y:actual[n][i/2].x)==field(std::to_string(n)+".point."+std::to_string(i)),"original hill subdivision point");
            }
            std::cout<<"128 original hill strip subdivisions compared\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    const auto panels_path=rr::golden::oracle_dir()+"/napa-hill-panels.tsv";
    if(std::filesystem::exists(panels_path)) {
        try {
            const auto golden=rr::golden::load(panels_path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.size()!=1)throw std::runtime_error("Wrong hill panel oracle");
            const auto& fields=golden.frames[0];
            rr::Assets assets((std::filesystem::path(RR_SOURCE_DIR).parent_path()/"cdrom/ROADRASH").string());
            const auto exe=assets.read("ROADRASH.EXE"),joins=rr::read_exe_data(exe,0x004688e0,128);
            const auto table=rr::read_depth_table(exe);
            rr::CelTexture texture;size_t total=0;
            for(int c=0;c<256;++c) {
                const std::string prefix="hill.panels."+std::to_string(c)+".";
                auto field=[&](const std::string& key) {const auto i=fields.find(prefix+key);if(i==fields.end() || i->second<INT32_MIN || i->second>INT32_MAX)throw std::runtime_error("Missing hill panel field: "+prefix+key);return int32_t(i->second);};
                rr::HillPanelInput input;input.side=uint8_t(field("side"));input.near.depth=field("near_depth");input.far.depth=field("far_depth");input.far.type=uint8_t(field("far_type"));input.occlusion_row=field("occlusion");input.logical_width=field("width");input.overlap=field("overlap")!=0;
                for(int i=0;i<16;++i) {auto word=[&](int p){uint32_t v=0;for(int b=0;b<4;++b)v|=uint32_t(joins[p+b])<<(b*8);return int32_t(v);};input.join_offsets[i]={word(i*8),word(i*8+4)};}
                for(int i=0;i<4;++i){auto point=[&](const std::string& kind){return rr::RasterPoint{field(kind+std::to_string(i*2)),field(kind+std::to_string(i*2+1))};};input.near.projected[i]=point("near.projected.");input.far.projected[i]=point("far.projected.");input.near.world[i]=point("near.world.");input.far.world[i]=point("far.world.");}
                const auto actual=rr::hill_draw_nodes(input,{field("camera_x"),field("camera_height")},table,[&](uint8_t,int32_t,int32_t){return &texture;});
                if(int(actual.size())!=field("count")){check(false,"original hill panel count");std::cerr<<"case "<<c<<": native "<<actual.size()<<" original "<<field("count")<<'\n';}
                total+=actual.size();
                for(size_t n=0;n<actual.size() && int(n)<field("count");++n) {
                    check(actual[n].flip_u==bool(field(std::to_string(n)+".flip_u")) && actual[n].flip_v==bool(field(std::to_string(n)+".flip_v")),"original hill panel flips");
                    for(int i=0;i<8;++i)if((i%2?actual[n].view_points[i/2].y:actual[n].view_points[i/2].x)!=field(std::to_string(n)+".point."+std::to_string(i))){check(false,"original hill panel point");std::cerr<<"case "<<c<<" node "<<n<<" component "<<i<<'\n';}
                }
            }
            std::cout<<"256 original hill panel cases compared, "<<total<<" nodes\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    const auto shoulder_path=rr::golden::oracle_dir()+"/napa-shoulders.tsv";
    if(std::filesystem::exists(shoulder_path)) {
        try {
            const auto golden=rr::golden::load(shoulder_path);
            if(golden.exe!="73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2" || golden.frames.size()!=1)throw std::runtime_error("Wrong shoulder oracle");
            const auto& fields=golden.frames[0];
            rr::Assets assets((std::filesystem::path(RR_SOURCE_DIR).parent_path()/"cdrom/ROADRASH").string());
            const auto table=rr::read_depth_table(assets.read("ROADRASH.EXE"));
            rr::CelTexture texture;size_t total=0;
            for(int c=0;c<256;++c) {
                const std::string prefix="shoulder."+std::to_string(c)+".";
                auto field=[&](const std::string& key) {const auto i=fields.find(prefix+key);if(i==fields.end() || i->second<INT32_MIN || i->second>INT32_MAX)throw std::runtime_error("Missing shoulder field: "+prefix+key);return int32_t(i->second);};
                rr::ShoulderPanelInput input;input.strip.side=uint8_t(field("side"));input.strip.far_depth=field("far_depth");input.near_depth=field("near_depth");input.logical_width=field("width");input.logical_height=field("height");input.has_lanes=field("lanes")!=0;
                for(int i=0;i<4;++i){input.strip.world[i]={field("world."+std::to_string(i*2)),field("world."+std::to_string(i*2+1))};input.strip.projected[i]={field("projected."+std::to_string(i*2)),field("projected."+std::to_string(i*2+1))};}
                const auto actual=rr::shoulder_draw_nodes(input,{field("camera_x"),field("camera_height")},table,[&](int32_t,int32_t){return &texture;});
                if(int(actual.size())!=field("count")){check(false,"original shoulder count");std::cerr<<"case "<<c<<": native "<<actual.size()<<" original "<<field("count")<<'\n';}
                total+=actual.size();
                for(size_t n=0;n<actual.size() && int(n)<field("count");++n) {
                    check(actual[n].flip_u==bool(field(std::to_string(n)+".flip_u")) && actual[n].flip_v==bool(field(std::to_string(n)+".flip_v")),"original shoulder flips");
                    for(int i=0;i<8;++i)if((i%2?actual[n].view_points[i/2].y:actual[n].view_points[i/2].x)!=field(std::to_string(n)+".point."+std::to_string(i))){check(false,"original shoulder point");std::cerr<<"case "<<c<<" node "<<n<<" component "<<i<<'\n';}
                }
            }
            std::cout<<"256 original shoulder cases compared, "<<total<<" nodes\n";
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    std::cout<<failures<<" failures\n";return failures?1:0;
}
