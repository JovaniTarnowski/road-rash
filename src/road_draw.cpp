#include "road_draw.hpp"

namespace rr {
namespace {
void quad(SDL_Renderer* r, float x0, float x1, float y0, float x2, float x3, float y1, SDL_FColor c) {
    const SDL_Vertex v[4]={{{x0,y0},c,{}},{{x1,y0},c,{}},{{x3,y1},c,{}},{{x2,y1},c,{}}};
    const int index[6]={0,1,2,0,2,3};
    SDL_RenderGeometry(r,nullptr,v,4,index,6);
}

}

void draw_road(SDL_Renderer* renderer, const std::vector<RoadSlice>& slices) {
    const float k=2.f/65536; // Q16 320x240 view space -> 640x480 presentation
    const SDL_Rect clip={0,0,640,480};
    SDL_SetRenderClipRect(renderer,&clip);
    SDL_SetRenderDrawColor(renderer,112,150,196,255);
    SDL_RenderFillRect(renderer,nullptr);
    auto x=[&](const RoadSlice& s,int r,int32_t o){ return float(s.screen_x(r,o))*k; };
    // Back to front; nearer slices paint over farther ones beyond crests. Only the route's
    // own roadway lays ground bands: the other half is projected from its own rise with no
    // vertical separation (0x00443d70), so its bands would cut across the rider's road.
    for (size_t n=slices.size(); n-- > 1;) {
        const auto& near=slices[n-1];
        const auto& far=slices[n];
        const int count=SDL_min(far.count,near.count);
        const bool band=(near.sample/4)%2;
        const int own=near.count==2 && far.count==2 ? near.own : 0;
        {
            const float y0=far.road[own].screen_y*k, y1=near.road[own].screen_y*k;
            if (y1>y0) quad(renderer,0,640,y0,0,640,y1,band?SDL_FColor{.24f,.45f,.18f,1}:SDL_FColor{.28f,.52f,.21f,1});
        }
        for (int i=0;i<count;++i) {
            const int r=count==2 ? (i==0 ? 1-own : own) : 0; // own roadway last, on top
            const auto& a=far.road[r];
            const auto& b=near.road[r];
            const float y0=a.screen_y*k, y1=b.screen_y*k;
            if (y1<=y0) continue; // back face beyond a crest
            quad(renderer,x(far,r,a.shoulder_left),x(far,r,a.shoulder_right),y0,
                 x(near,r,b.shoulder_left),x(near,r,b.shoulder_right),y1,
                 band?SDL_FColor{.55f,.50f,.38f,1}:SDL_FColor{.60f,.55f,.42f,1});
            quad(renderer,x(far,r,a.left),x(far,r,a.right),y0,x(near,r,b.left),x(near,r,b.right),y1,
                 band?SDL_FColor{.40f,.40f,.42f,1}:SDL_FColor{.45f,.45f,.47f,1});
            if (!band) continue;
            // Lane boundaries every 250 world units from the centre line (0x00454240).
            for (int32_t o=b.left/250*250; o<b.right; o+=250) {
                if (o<=b.left || o<=a.left || o>=a.right) continue;
                const int32_t w=12;
                quad(renderer,x(far,r,o-w),x(far,r,o+w),y0,x(near,r,o-w),x(near,r,o+w),y1,{.92f,.92f,.86f,1});
            }
        }
    }
    SDL_SetRenderClipRect(renderer,nullptr);
}

}
