#include "import_ui.hpp"
#include "importer.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
namespace rr {
namespace {
struct ImportUI {
    std::mutex mutex;
    std::string selected,message;
    ImportProgress progress{};
    std::atomic<bool> dialog_pending{false},working{false},complete{false},cancel{false};
};
void SDLCALL selected_file(void* data,const char* const* files,int) {
    std::unique_ptr<std::shared_ptr<ImportUI>> owner(static_cast<std::shared_ptr<ImportUI>*>(data));
    const auto state=*owner;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!files) state->message=std::string("File selection failed: ")+SDL_GetError();
        else if (files[0]) { state->selected=files[0]; state->message.clear(); }
    }
    state->dialog_pending=false;
}
}
bool run_import_screen(SDL_Window* window,SDL_Renderer* renderer,const std::string& destination,
                       const std::string& screenshot,bool smoke) {
    const auto state=std::make_shared<ImportUI>();
    std::thread worker;
    bool running=true;
    static const SDL_DialogFileFilter filter{"Disc image","iso"};
    auto dialog=[&](bool folder) {
        if (state->working || state->dialog_pending) return;
        state->dialog_pending=true;
        auto* owner=new std::shared_ptr<ImportUI>(state);
        if (folder) SDL_ShowOpenFolderDialog(selected_file,owner,window,nullptr,false);
        else SDL_ShowOpenFileDialog(selected_file,owner,window,&filter,1,nullptr,false);
    };
    while (running && !state->complete) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type==SDL_EVENT_QUIT) running=false;
            if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key==SDLK_ESCAPE || event.key.key==SDLK_AC_BACK || event.key.key==SDLK_Q) running=false;
                if (event.key.key==SDLK_I || event.key.key==SDLK_RETURN) dialog(false);
#ifndef __ANDROID__
                if (event.key.key==SDLK_F) dialog(true);
#endif
            }
            if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_FINGER_DOWN) {
                float x,y;
                if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN) { x=event.button.x; y=event.button.y; }
                else { int w,h; SDL_GetWindowSize(window,&w,&h); x=event.tfinger.x*w; y=event.tfinger.y*h; }
                float logical_x,logical_y;
                SDL_RenderCoordinatesFromWindow(renderer,x,y,&logical_x,&logical_y);
                if (logical_y>=200 && logical_y<=250) {
                    if (logical_x>=80 && logical_x<=300) dialog(false);
#ifndef __ANDROID__
                    if (logical_x>=340 && logical_x<=560) dialog(true);
#endif
                }
            }
        }
        std::string selection;
        { std::lock_guard<std::mutex> lock(state->mutex); selection.swap(state->selected); }
        if (!selection.empty() && !state->working) {
            if (worker.joinable()) worker.join();
            state->working=true;
            worker=std::thread([state,selection,destination]() {
                const auto result=import_game(selection,destination,[state](const ImportProgress& p) {
                    if (state->cancel) throw std::runtime_error("Import canceled");
                    std::lock_guard<std::mutex> lock(state->mutex); state->progress=p;
                });
                { std::lock_guard<std::mutex> lock(state->mutex);
                  state->message=result.error;
                  if (!result.missing.empty()) state->message="Missing: "+result.missing.front();
                  if (!result.mismatched.empty()) state->message="Hash mismatch: "+result.mismatched.front();
                }
                state->complete=result.ok;
                state->working=false;
            });
        }
        // Road Rash arcade treatment on the 640x480 logical canvas. Input
        // geometry (button hit rects) is unchanged; only the paint is new.
        const uint32_t ticks=SDL_GetTicks();
        auto fill=[&](float x,float y,float w,float h,uint8_t r,uint8_t g,uint8_t b,uint8_t a=255) {
            SDL_SetRenderDrawColor(renderer,r,g,b,a);
            const SDL_FRect rect{x,y,w,h}; SDL_RenderFillRect(renderer,&rect);
        };
        auto inked_text=[&](float x,float y,const char* text,uint8_t r,uint8_t g,uint8_t b) {
            SDL_SetRenderDrawColor(renderer,r,g,b,255); SDL_RenderDebugText(renderer,x,y,text);
        };
        fill(0,0,640,480,11,12,16);
        for (int y=44;y<440;y+=6) fill(0,float(y),640,1,17,19,25);
        // Horizon glow behind the title.
        for (int i=0;i<24;++i) fill(0,float(52+i),640,1,uint8_t(26+i),uint8_t(18+i/2),uint8_t(22+i/3));
        // Top checkered flag + red stripe.
        for (int row=0;row<2;++row)
            for (int x=0;x<40;++x) {
                const bool light=((x+row)&1)==0;
                fill(float(x*16),float(row*16),16,16,light?uint8_t(232):uint8_t(8),light?uint8_t(230):uint8_t(9),light?uint8_t(224):uint8_t(12));
            }
        fill(0,32,640,6,200,22,22); fill(0,38,640,2,255,196,0);
        // Leader dashes racing under the flag (animated).
        const int dash_off=int(ticks/28)%64;
        for (int x=-64;x<640+64;x+=64) fill(float(x+dash_off),42,34,3,255,196,0);
        // Title with hard offset shadow: red slab, then yellow face.
        SDL_SetRenderScale(renderer,4,4);
        SDL_SetRenderDrawColor(renderer,200,22,22,255);
        SDL_RenderDebugText(renderer,(640.f-9*8*4)/2.f/4.f+1.25f,64/4.f+0.75f,"ROAD RASH");
        SDL_SetRenderDrawColor(renderer,255,196,0,255);
        SDL_RenderDebugText(renderer,(640.f-9*8*4)/2.f/4.f,64/4.f,"ROAD RASH");
        SDL_SetRenderScale(renderer,1,1);
        // NATIVE plate.
        fill(210,116,220,34,255,196,0);
        fill(210,116,220,3,200,22,22);
        fill(210,147,220,3,20,20,22);
        SDL_SetRenderScale(renderer,2,2);
        SDL_SetRenderDrawColor(renderer,16,16,18,255);
        SDL_RenderDebugText(renderer,(640.f-6*8*2)/2.f/2.f,122/2.f,"NATIVE");
        SDL_SetRenderScale(renderer,1,1);
        // Speed streaks flanking the plate.
        fill(80,128,90,4,200,22,22); fill(80,136,60,3,232,230,224);
        fill(470,128,90,4,200,22,22); fill(500,136,60,3,232,230,224);
        inked_text(80,168,"Import your Road Rash disc to begin.",232,230,224);
        inked_text(80,187,"Use the 1997 Windows rerelease ISO or extracted folder.",150,156,168);
        // Buttons keep their logical hit rects: ISO {80,200,220,50}.
        const SDL_FRect iso_button{80,200,220,50};
        const bool blink=(ticks/500)%2==0;
        fill(80,200,220,50,255,196,0);
        fill(80,200,220,5,200,22,22);
        fill(80,245,220,5,20,20,22);
        if (blink) { SDL_SetRenderDrawColor(renderer,255,255,255,255); SDL_RenderRect(renderer,&iso_button); }
        else { SDL_SetRenderDrawColor(renderer,120,80,0,255); SDL_RenderRect(renderer,&iso_button); }
        inked_text(106,221,"I / Enter: choose ISO",16,16,18);
#ifndef __ANDROID__
        const SDL_FRect folder_button{340,200,220,50};
        fill(340,200,220,50,26,30,40);
        fill(340,200,220,5,150,156,168);
        fill(340,245,220,5,200,22,22);
        { SDL_SetRenderDrawColor(renderer,232,230,224,255); SDL_RenderRect(renderer,&folder_button); }
        inked_text(375,221,"F: choose folder",232,230,224);
#endif
        // Road strip under the buttons.
        fill(0,262,640,2,60,64,74);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->working) {
                const auto& p=state->progress;
                const std::string text="Verifying "+std::to_string(p.done)+" / "+std::to_string(p.total);
                inked_text(80,285,text.c_str(),255,196,0);
                inked_text(80,308,p.current.c_str(),150,156,168);
                const float frac=p.total?float(p.done)/float(p.total):0.f;
                fill(80,330,480,16,28,30,38);
                const float w=480.f*std::min(1.f,std::max(0.f,frac));
                if (w>0) fill(81,331,w,14,200,22,22);
                if (w>8) fill(81+w-8,331,8,14,255,196,0);
                SDL_SetRenderDrawColor(renderer,232,230,224,255);
                const SDL_FRect track{80,330,480,16}; SDL_RenderRect(renderer,&track);
            } else if (state->dialog_pending) inked_text(80,285,"Choose your disc image in the file picker.",255,196,0);
            else if (!state->message.empty()) {
                inked_text(80,285,state->message.substr(0,62).c_str(),255,120,100);
                inked_text(80,308,"Import did not complete. Choose the correct disc to retry.",150,156,168);
            } else {
                if (blink) inked_text(80,285,"Press I or click the yellow button. F takes a folder.",255,196,0);
                else inked_text(80,285,"Press I or click the yellow button. F takes a folder.",110,116,128);
            }
        }
        // Footer: red stripe + checkered flag.
        inked_text(80,404,"FULL THROTTLE - NO BRAKES",90,96,108);
        fill(0,424,640,5,200,22,22); fill(0,429,640,2,255,196,0);
        for (int row=0;row<2;++row)
            for (int x=0;x<40;++x) {
                const bool light=((x+row)&1)==0;
                fill(float(x*16),float(431+row*16),16,16,light?uint8_t(232):uint8_t(8),light?uint8_t(230):uint8_t(9),light?uint8_t(224):uint8_t(12));
            }
        fill(0,463,640,17,11,12,16);
        if (!screenshot.empty()) {
            SDL_Surface* pixels=SDL_RenderReadPixels(renderer,nullptr);
            if (!pixels || !SDL_SaveBMP(pixels,screenshot.c_str())) {
                SDL_DestroySurface(pixels);
                state->cancel=true;
                if (worker.joinable()) worker.join();
                throw std::runtime_error(SDL_GetError());
            }
            SDL_DestroySurface(pixels);
        }
        SDL_RenderPresent(renderer);
        if (smoke || !screenshot.empty()) break;
        SDL_Delay(16);
    }
    state->cancel=true;
    if (worker.joinable()) worker.join();
    return state->complete;
}
}
