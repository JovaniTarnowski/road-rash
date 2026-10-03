#include "import_ui.hpp"
#include "importer.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
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
        SDL_SetRenderDrawColor(renderer,15,18,22,255); SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_SetRenderScale(renderer,2,2);
        SDL_RenderDebugText(renderer,55,42,"ROAD RASH NATIVE");
        SDL_SetRenderScale(renderer,1,1);
        SDL_RenderDebugText(renderer,80,145,"Import your Road Rash disc to begin.");
        SDL_RenderDebugText(renderer,80,166,"Use the 1997 Windows rerelease ISO or extracted folder.");
        const SDL_FRect iso_button{80,200,220,50};
        SDL_SetRenderDrawColor(renderer,45,90,140,255); SDL_RenderFillRect(renderer,&iso_button);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderDebugText(renderer,108,221,"I / Enter: choose ISO");
#ifndef __ANDROID__
        const SDL_FRect folder_button{340,200,220,50};
        SDL_SetRenderDrawColor(renderer,45,90,140,255); SDL_RenderFillRect(renderer,&folder_button);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderDebugText(renderer,375,221,"F: choose folder");
#endif
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->working) {
                const auto& p=state->progress;
                const std::string text="Verifying "+std::to_string(p.done)+" / "+std::to_string(p.total);
                SDL_RenderDebugText(renderer,80,285,text.c_str());
                SDL_RenderDebugText(renderer,80,308,p.current.c_str());
            } else if (state->dialog_pending) SDL_RenderDebugText(renderer,80,285,"Choose your disc image in the file picker.");
            else if (!state->message.empty()) {
                SDL_SetRenderDrawColor(renderer,255,150,125,255);
                SDL_RenderDebugText(renderer,80,285,state->message.substr(0,62).c_str());
                SDL_RenderDebugText(renderer,80,308,"Import did not complete. Choose the correct disc to retry.");
            }
        }
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
