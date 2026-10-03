/* Oracle hook DLL for ROADRASH.EXE (SHA-256 73d7f974...). Replaces AWEMAN32.DLL.
   The game statically imports AWEMAN32.dll ordinal 2; this DLL forwards it to the
   renamed original (AWEMAN_REAL.DLL) and patches call sites to dump game state. */
#include <stdint.h>
#include <string.h>

/* Verify an E8 rel32 call at site targeting expected_target, then redirect it to hook. */
static int rr_patch_bytes(uint8_t* site, uint32_t expected_target, void* hook) {
    int32_t rel;
    if (site[0] != 0xE8) return 0;
    memcpy(&rel, site + 1, 4);
    if ((uint32_t)((uintptr_t)(site + 5) + rel) != expected_target) return 0;
    rel = (int32_t)((uintptr_t)hook - (uintptr_t)(site + 5));
    memcpy(site + 1, &rel, 4);
    return 1;
}

#ifdef RR_PATCH_NO_WINDOWS
int rr_patch_call(uint8_t* site, uint32_t expected_target, void* hook) { return rr_patch_bytes(site, expected_target, hook); }
#else
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* g_out;
static char g_scenario[256] = "unnamed";

/* Ordinal 2 of the original AWEMAN32.DLL, loaded from AWEMAN_REAL.DLL.
   GNU ld cannot forward by ordinal, so export a jump trampoline instead. */
void* g_real2;
__attribute__((naked)) void Forward2(void) { __asm__("jmp *_g_real2"); }

int rr_patch_call(uint8_t* site, uint32_t expected_target, void* hook) {
    DWORD old;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    int ok = rr_patch_bytes(site, expected_target, hook);
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return ok;
}

static void env(const char* name, char* buf, DWORD size, const char* fallback) {
    DWORD count = GetEnvironmentVariableA(name, buf, size);
    if (!count || count >= size) { strncpy(buf, fallback, size - 1); buf[size - 1] = 0; }
}

void rr_log(const char* key, long long value) { if (g_out) fprintf(g_out, "%s\t%lld\n", key, value); }

typedef void (__cdecl *rr_frame_fn)(void);
static rr_frame_fn const orig_frame = (rr_frame_fn)0x004439f0;
static uint32_t g_frame;
static int g_quit_after = -1;
static int g_pixels, g_pixel_every=30, g_pixel_written;
static int g_pixel_width, g_pixel_height, g_pixel_pitch;
static char g_output[MAX_PATH];
static int g_raster;
static struct { int vertices[8],input[8],clip[4],colour,width,height; } g_raster_cases[64];
static int g_raster_count;
static int g_view_inputs[12]={0,65535,65536,65537,-1,-65535,-65536,-65537,32768,-32768,0x7fffffff,(int)0x80000000u};
static int g_view_outputs[2][12];
#define RD32(a) (*(volatile int32_t*)(uintptr_t)(a))
#define RD8(a) (*(volatile uint8_t*)(uintptr_t)(a))

static int readable(uintptr_t p, size_t size) {
    MEMORY_BASIC_INFORMATION info;
    if (!p || !VirtualQuery((void*)p, &info, sizeof info)) return 0;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    return size <= (uintptr_t)info.BaseAddress + info.RegionSize - p;
}

// Isolated synthetic inputs exercise the original filler. These images contain
// no disc graphics and are kept as local oracle output, not embedded goldens.
static void capture_raster(uintptr_t base,int width,int height,int pitch) {
    static const int fixture[8][8]={
        {1,1,5,1,5,5,1,5}, {3,0,6,3,3,6,0,3},
        {-20,40,100,40,60,40,0,40}, {-40,-30,80,-30,80,30,-40,30},
        {20,20,20,20,120,120,20,120}, {50,20,100,50,50,130,10,70},
        {200,20,500,35,450,170,150,155}, {-10,479,650,479,650,479,-10,479}
    };
    int saved[8];
    const uint32_t globals[8]={0x004b7eb0,0x004b7eb4,0x004b7eb8,0x004b7ebc,0x004c5d60,0x004c5d64,0x004c5d68,0x004c5d6c};
    for (int i=0;i<8;++i) saved[i]=RD32(globals[i]);
    uint32_t seed=1;
    for (int c=0;c<64;++c) {
        int vertices[8];
        if (c<8) memcpy(vertices,fixture[c],sizeof vertices);
        else {
            int values[6];
            for (int i=0;i<6;++i) { seed=seed*1664525u+1013904223u;values[i]=(int)(seed>>16); }
            int top=values[0]%900-300,bottom=top+1+values[1]%500;
            int left_top=values[2]%1200-300,left_bottom=values[3]%1200-300;
            int generated[8]={left_top,top,left_top+1+values[4]%800,top,left_bottom+1+values[5]%800,bottom,left_bottom,bottom};
            for (int i=0;i<4;++i) { int v=(i+c)%4;vertices[i*2]=generated[v*2];vertices[i*2+1]=generated[v*2+1]; }
            if (c%7==0) { int v=(c/7)%4,j=(v+1)%4;vertices[v*2]=vertices[j*2];vertices[v*2+1]=vertices[j*2+1]; }
        }
        if (c==62) { int extreme[8]={-65536,-65536,65534,-65536,65534,65534,-65536,65534};memcpy(vertices,extreme,sizeof extreme); }
        if (c==63) { int huge[8]={-16384,-16384,16384,-16384,16384,16384,-16384,16384};memcpy(vertices,huge,sizeof huge); }
        for (int row=0;row<height;++row) memset((void*)(base+(size_t)row*pitch),0,width);
        int clip[4]={0,0,width,height-1};
        if (c==3) { clip[0]=7;clip[1]=3;clip[2]=60;clip[3]=25; }
        int minx=vertices[0],maxx=minx,miny=vertices[1],maxy=miny;
        for (int v=1;v<4;++v) {
            if (vertices[2*v]<minx) minx=vertices[2*v];
            if (vertices[2*v]>maxx) maxx=vertices[2*v];
            if (vertices[2*v+1]<miny) miny=vertices[2*v+1];
            if (vertices[2*v+1]>maxy) maxy=vertices[2*v+1];
        }
        for (int i=0;i<4;++i) RD32(globals[i])=clip[i];
        RD32(0x004c5d60)=minx;RD32(0x004c5d64)=miny;RD32(0x004c5d68)=maxx;RD32(0x004c5d6c)=maxy;
        memcpy(g_raster_cases[c].input,vertices,sizeof vertices);
        const int colour=20+c;
        ((void (__fastcall*)(int*,unsigned char))0x0044d620)(vertices,(unsigned char)colour);
        memcpy(g_raster_cases[c].vertices,vertices,sizeof vertices);memcpy(g_raster_cases[c].clip,clip,sizeof clip);
        g_raster_cases[c].colour=colour;g_raster_cases[c].width=width;g_raster_cases[c].height=height;
        char path[1024];const char* slash=strrchr(g_output,'\\');
        const size_t prefix=slash ? (size_t)(slash-g_output)+1 : 0;
        if (prefix>sizeof path-64) continue;
        memcpy(path,g_output,prefix);
        int count=snprintf(path+prefix,sizeof path-prefix,"%s-raster%u.raw",g_scenario,(unsigned)c);
        if (count<0 || (size_t)count>=sizeof path-prefix) continue;
        FILE* output=fopen(path,"wb");if (!output) continue;
        int ok=1;for (int row=0;row<height;++row) if (fwrite((void*)(base+(size_t)row*pitch),1,width,output)!=(size_t)width) { ok=0;break; }
        if (fclose(output)) ok=0;
        if (ok) g_raster_count=c+1;
    }
    for (int i=0;i<8;++i) RD32(globals[i])=saved[i];
}

// 0x0044d300 locks the surface while drawing and stores its view-origin pixel
// pointer/pitch at 0x004b7ec0/0x004b7ec4. Capture before 0x0044c780 unlocks it.
static void __cdecl hook_draw(void) {
    ((rr_frame_fn)0x00450420)();
    g_pixel_written=0;
    if (!g_out || !RD8(0x00475620)) return;
    const uintptr_t base=(uint32_t)RD32(0x004b7ec0);
    const int width=RD32(0x0047539c), height=RD32(0x004753a0), pitch=RD32(0x004b7ec4);
    if (width<1 || width>1280 || height<1 || height>960 || pitch<width || pitch>16384) return;
    if (!readable(base,(size_t)(height-1)*pitch+width)) return;
    if (g_raster && g_frame==0) capture_raster(base,width,height,pitch);
    if (!g_pixels || g_frame % (uint32_t)g_pixel_every) return;
    char path[MAX_PATH];
    const char* slash=strrchr(g_output,'\\');
    const size_t prefix=slash ? (size_t)(slash-g_output)+1 : 0;
    if (prefix>sizeof path-64) return;
    memcpy(path,g_output,prefix);
    int count=snprintf(path+prefix,sizeof path-prefix,"%s-f%u.raw",g_scenario,g_frame);
    if (count<0 || (size_t)count>=sizeof path-prefix) return;
    FILE* pixels=fopen(path,"wb");
    if (!pixels) return;
    int ok=1;
    for (int row=0;row<height;++row)
        if (fwrite((void*)(base+(size_t)row*pitch),1,width,pixels)!=(size_t)width) { ok=0; break; }
    if (fclose(pixels)) ok=0;
    if (ok) {
        g_pixel_written=1;
        g_pixel_width=width; g_pixel_height=height; g_pixel_pitch=pitch;
    }
}

static void dump_raster(void) {
    char key[96];
    if (g_raster_count) {
        rr_log("raster.count",g_raster_count);
        for (int c=0;c<g_raster_count;++c) {
            snprintf(key,sizeof key,"raster.%d.width",c);rr_log(key,g_raster_cases[c].width);
            snprintf(key,sizeof key,"raster.%d.height",c);rr_log(key,g_raster_cases[c].height);
            snprintf(key,sizeof key,"raster.%d.colour",c);rr_log(key,g_raster_cases[c].colour);
            for (int i=0;i<8;++i) {
                snprintf(key,sizeof key,"raster.%d.vertex.%d",c,i);rr_log(key,g_raster_cases[c].vertices[i]);
                snprintf(key,sizeof key,"raster.%d.input.%d",c,i);rr_log(key,g_raster_cases[c].input[i]);
            }
            for (int i=0;i<4;++i) { snprintf(key,sizeof key,"raster.%d.clip.%d",c,i);rr_log(key,g_raster_cases[c].clip[i]); }
        }
    }
}

static void capture_view(void) {
    const int shift=RD32(0x00481fa8),mask=RD32(0x00481ff4);
    for (int mode=0;mode<2;++mode) {
        RD32(0x00481fa8)=mode ? 15 : 16;
        RD32(0x00481ff4)=mode ? (int)0xfffffffeu : -1;
        for (int i=0;i<12;++i) {
            int source[32]={0},destination[32]={0};
            // No sprite pointer: 0x00410080 returns after copying null.
            source[0x44/4]=g_view_inputs[i];
            ((void (__fastcall*)(int*,int*))0x0040ffc0)(destination,source);
            g_view_outputs[mode][i]=destination[0x18/4];
        }
    }
    RD32(0x00481fa8)=shift;RD32(0x00481ff4)=mask;
    char key[96];
    for (int mode=0;mode<2;++mode) for (int i=0;i<12;++i) {
        snprintf(key,sizeof key,"view.map.%d.%d.input",mode,i);rr_log(key,g_view_inputs[i]);
        snprintf(key,sizeof key,"view.map.%d.%d.output",mode,i);rr_log(key,g_view_outputs[mode][i]);
    }
}

static void terrain_word(unsigned char* b,int p,uint32_t v) { memcpy(b+p,&v,4); }
static void capture_terrain(void) {
    const int prior_mode=RD32(0x004753b0),prior_random=RD32(0x0049bce8);
    RD32(0x004753b0)=3; // original rand wrapper returns supplied nonnegative value
    const int values[3]={0,123,32767};
    char key[128];
    for(int kind=0;kind<3;++kind) for(int random=0;random<3;++random) for(int reverse=0;reverse<2;++reverse) {
        unsigned char b[248]={0};int iterator[80]={0},output[100]={0};
        const int size=kind==0?144:248;
        memcpy(b,kind==0?"DLBR":kind==1?"NTMR":"LIHR",4);
        terrain_word(b,4,size);terrain_word(b,8,kind==2?3:64);
        int selectors=16;
        if(kind==1) {
            terrain_word(b,12,3);terrain_word(b,16,3);selectors=120;
            for(int side=0;side<2;++side) for(int j=0;j<3;++j) {
                const int p=24+(side*3+j)*16;
                const unsigned char params[8]={0,(unsigned char)(3+j),(unsigned char)(20+side*5),4,30,6,5,40};
                terrain_word(b,p,j==2?64:j*20);memcpy(b+p+4,params,8);terrain_word(b,p+12,side?-40:70);
            }
        } else if(kind==2) {
            for(int j=0;j<3;++j) {
                const int p=20+76*j;terrain_word(b,p,j*20);terrain_word(b,p+4,(j+1)*20);
                b[p+8]=10+j;b[p+9]=20+j;
                for(int point=0;point<8;++point) {
                    const unsigned char params[8]={(unsigned char)(10+point*12),(unsigned char)(20+point*12),3,(unsigned char)(-10+point),(unsigned char)(12+point),4,1,5};
                    memcpy(b+p+12+8*point,params,8);
                }
            }
        }
        if(kind!=2) for(int j=0;j<64;++j) {b[selectors+2*j]=j;b[selectors+2*j+1]=63-j;}
        RD32(0x0049bce8)=values[random];
        ((void (__fastcall*)(int*,void*,int))0x004510c0)(iterator,b,reverse?48*256:0);
        for(int tick=0;tick<48;++tick) {
            memset(output,0,sizeof output);output[0xe8/4]=-500;output[0xec/4]=500;
            ((void (__fastcall*)(int*,int*))0x00451910)(iterator,output);
            snprintf(key,sizeof key,"terrain.%d.%d.%d.%d.type",kind,random,reverse,tick);rr_log(key,((unsigned char*)output)[0x3c]);
            for(int side=0;side<2;++side) {
                snprintf(key,sizeof key,"terrain.%d.%d.%d.%d.selector.%d",kind,random,reverse,tick,side);rr_log(key,((unsigned char*)output)[0x148+side]);
            }
            if(kind==1) for(int j=0;j<6;++j) {
                snprintf(key,sizeof key,"terrain.%d.%d.%d.%d.wall.%d",kind,random,reverse,tick,j);rr_log(key,output[0x14c/4+j]);
            }
            if(kind==2) for(int j=0;j<12;++j) {
                snprintf(key,sizeof key,"terrain.%d.%d.%d.%d.point.%d",kind,random,reverse,tick,j);rr_log(key,output[0xf0/4+j]);
            }
            if(kind==2) for(int j=0;j<6;++j) {
                snprintf(key,sizeof key,"terrain.%d.%d.%d.%d.shade.%d",kind,random,reverse,tick,j);rr_log(key,output[0x120/4+j]);
            }
            ((void (__fastcall*)(int*,int))0x004516e0)(iterator,reverse?-256:256);
        }
    }
    RD32(0x004753b0)=prior_mode;RD32(0x0049bce8)=prior_random;
}

static void capture_hill_projection(void) {
    const int old_x=RD32(0x004bc7d8),old_height=RD32(0x004bc7dc),old_width=RD32(0x0046667c);
    const int scales[6]={32768,65535,65536,98304,163840,0x7fffffff};
    const int positions[8]={0,-600,600,-40000,40000,0x7fffffff,(int)0x80000000u,1200};
    char key[96];
    for(int c=0;c<96;++c) {
        int source[100]={0},road[100]={0};
        road[0xc/4]=(int)(uintptr_t)source;road[0x10/4]=positions[c%8];road[0x14/4]=(c%5)*80-160;
        road[0x20/4]=scales[c/16];
        RD32(0x004bc7d8)=positions[(c+3)%8];RD32(0x004bc7dc)=150+c*17;RD32(0x0046667c)=c%2?321:320;
        road[0x4c/4]=(int)((uint32_t)(RD32(0x004bc7dc)-road[0x14/4])*(uint32_t)road[0x20/4]+0x5a0000u);
        for(int i=0;i<6;++i) {source[0xf0/4+i*2]=(i%2?1:-1)*(i*200+c*80);source[0xf0/4+i*2+1]=i*37-c*11;}
        const int row=((int (__fastcall*)(int*))0x0041d6d0)(road);
#define HILL_LOG(label,value) do {snprintf(key,sizeof key,"hill.project.%d.%s",c,label);rr_log(key,value);} while(0)
        HILL_LOG("world_x",road[0x10/4]);HILL_LOG("world_y",road[0x14/4]);
        HILL_LOG("camera_x",RD32(0x004bc7d8));HILL_LOG("camera_height",RD32(0x004bc7dc));
        HILL_LOG("width",RD32(0x0046667c));HILL_LOG("scale",road[0x20/4]);HILL_LOG("maximum_row",row);
#undef HILL_LOG
        for(int i=0;i<12;++i) {
            snprintf(key,sizeof key,"hill.project.%d.input.%d",c,i);rr_log(key,source[0xf0/4+i]);
            snprintf(key,sizeof key,"hill.project.%d.output.%d",c,i);rr_log(key,road[0x70/4+i]);
        }
    }
    RD32(0x004bc7d8)=old_x;RD32(0x004bc7dc)=old_height;RD32(0x0046667c)=old_width;
}

static void capture_polygon(void) {
    const int prior_node=RD32(0x004c5d88);
    uint32_t random=1;char key[96];
    for(int c=0;c<2048;++c) {
        int points[8],source[8];unsigned char node[80]={0};
        for(int i=0;i<8;++i) {random=random*1664525u+1013904223u;points[i]=(int)(random>>16)%2001-1000;}
        if(c%7==0) {points[2]=points[0];points[3]=points[1];}
        if(c%11==0) for(int i=0;i<4;++i) points[2*i+1]=10;
        if(c%13==0) for(int i=0;i<8;++i)points[i]*=60;
        const int flags=c%3==0?0:c%3==1?8:4;
        node[0x3d]=c%2;RD32(0x004c5d88)=(int)(uintptr_t)node;memcpy(source,points,sizeof points);
        int accepted=((int (__fastcall*)(int*,int))0x0044ce60)(points,flags)&255;
        snprintf(key,sizeof key,"polygon.%d.flags",c);rr_log(key,flags);
        snprintf(key,sizeof key,"polygon.%d.accepted",c);rr_log(key,accepted);
        snprintf(key,sizeof key,"polygon.%d.flip",c);rr_log(key,node[0x3d]);
        for(int i=0;i<8;++i) {
            snprintf(key,sizeof key,"polygon.%d.input.%d",c,i);rr_log(key,source[i]);
            snprintf(key,sizeof key,"polygon.%d.output.%d",c,i);rr_log(key,points[i]);
        }
    }
    RD32(0x004c5d88)=prior_node;
}

static void capture_texture_spans(void) {
    const int prior_texture=RD32(0x004c5d20),prior_output=RD32(0x004c5d50);
    const uint32_t functions[2][9]={{0x418910,0x418bb0,0x418e50,0x4190f0,0x419390,0x419630,0x4198d0,0x419b70,0x419e10},
        {0x41a0b0,0x41a420,0x41a780,0x41aaf0,0x41ae60,0x41b1d0,0x41b540,0x41b8b0,0x41bc20}};
    const int counts[7]={0,1,2,3,4,7,17};
    unsigned char* texture=malloc(8192);if(!texture)return;
    for(int i=0;i<8192;++i)texture[i]=(unsigned char)(i*37+i/7);
    RD32(0x004c5d20)=(int)(uintptr_t)texture;
    char key[96];int c=0;
    for(int transparent=0;transparent<2;++transparent) for(int shift=0;shift<9;++shift)
    for(int alignment=0;alignment<4;++alignment) for(int length=0;length<7;++length) for(int moving=0;moving<2;++moving,++c) {
        uint32_t output[16];memset(output,125,sizeof output);
        const int row=8*65536+(c%2?65535:32767),column=(1<<shift)/2*65536+12345;
        const int row_step=moving?(c%3?2048:-768):0,column_step=c%2?512:-128;
        RD32(0x004c5d50)=(int)(uintptr_t)((unsigned char*)output+alignment);
        ((void (__fastcall*)(int,int,int,int,int))(uintptr_t)functions[transparent][shift])(counts[length],row,column,column_step,row_step);
#define SPAN_LOG(label,value) do {snprintf(key,sizeof key,"texture.span.%d.%s",c,label);rr_log(key,value);} while(0)
        SPAN_LOG("transparent",transparent);SPAN_LOG("shift",shift);SPAN_LOG("alignment",alignment);SPAN_LOG("count",counts[length]);
        SPAN_LOG("row",row);SPAN_LOG("column",column);SPAN_LOG("row_step",row_step);SPAN_LOG("column_step",column_step);
#undef SPAN_LOG
        for(int i=0;i<32;++i){snprintf(key,sizeof key,"texture.span.%d.output.%d",c,i);rr_log(key,((unsigned char*)output)[i]);}
    }
    RD32(0x004c5d20)=prior_texture;RD32(0x004c5d50)=prior_output;free(texture);
}

static void capture_texture_quads(void) {
    const uint32_t addresses[12]={0x4b7eb0,0x4b7eb4,0x4b7eb8,0x4b7ebc,0x4b7ec0,0x4b7ec4,0x4c5d88,0x4c5d60,0x4c5d64,0x4c5d68,0x4c5d6c,0x484278};
    int saved[12];for(int i=0;i<12;++i)saved[i]=RD32(addresses[i]);
    unsigned char* pixels=malloc(640*480);unsigned char* texture=malloc(1344);
    unsigned char* edges=malloc(11520);unsigned char uv_saved[32];
    const int prior_texture=RD32(0x4c5d20),prior_output=RD32(0x4c5d50);
    if(!pixels || !texture || !edges){free(pixels);free(texture);free(edges);return;}
    memcpy(edges,(void*)0x484280,5760);memcpy(edges+5760,(void*)0x482bf8,5760);memcpy(uv_saved,(void*)0x4c5d30,32);
    for(int level=0;level<3;++level){int offset=level==0?0:level==1?1024:1280,size=level==0?1024:level==1?256:64;
        for(int i=0;i<size;++i)texture[offset+i]=(unsigned char)(i*37+i/7+level*53);}
    RD32(0x4b7eb0)=0;RD32(0x4b7eb4)=0;RD32(0x4b7eb8)=640;RD32(0x4b7ebc)=479;
    RD32(0x4b7ec0)=(int)(uintptr_t)pixels;RD32(0x4b7ec4)=640;
    char key[96];
    for(int c=0;c<64;++c) {
        int resource[32]={0},node[20]={0};short vertices[16];
        resource[3]=c%2?2:0;resource[0x20/4]=32;resource[0x28/4]=3;resource[0x38/4]=(int)(uintptr_t)texture;
        resource[0x44/4]=(int)(uintptr_t)(texture+1024);resource[0x50/4]=(int)(uintptr_t)(texture+1280);
        node[1]=(int)(uintptr_t)resource;node[2]=0;node[3]=0;node[4]=32;node[5]=32;
        RD32(0x4c5d88)=(int)(uintptr_t)node;
        int top=c%8*70-80,bottom=top+20+(c*31)%400,left=c%5*90-100,right=left+20+(c*67)%700;
        if(c>=48){left=20+30*(c%5);right=left+1+(c*7)%25;bottom=top+1+(c*11)%25;}
        if(c==50 || c==51){left=80;top=80;right=86;bottom=86;}
        int points[8]={left,top,right,top,right-(c%4)*((right-left)/8),bottom,left+(c%3)*((right-left)/9),bottom};
        if(c%7==0 && c<48){points[3]=top+9;points[7]=bottom-11;}
        RD32(0x4c5d60)=left;RD32(0x4c5d68)=right;RD32(0x4c5d64)=top;RD32(0x4c5d6c)=bottom;
        for(int i=0;i<4;++i){vertices[i*4]=(short)points[i*2];vertices[i*4+1]=(short)points[i*2+1];vertices[i*4+2]=i<2?1:31;vertices[i*4+3]=i==0||i==3?1:31;}
        memcpy((void*)0x4c5d30,vertices,sizeof vertices);memset(pixels,125,640*480);
        ((void (__cdecl*)(void))0x4180b0)();
        int level=RD32(0x4c5d20)==(int)(uintptr_t)texture?0:RD32(0x4c5d20)==(int)(uintptr_t)(texture+1024)?1:2;
        snprintf(key,sizeof key,"texture.quad.%d.level",c);rr_log(key,level);
        snprintf(key,sizeof key,"texture.quad.%d.transparent",c);rr_log(key,c%2);
        for(int i=0;i<16;++i){snprintf(key,sizeof key,"texture.quad.%d.vertex.%d",c,i);rr_log(key,(unsigned short)vertices[i]);}
        char path[1024];const char* slash=strrchr(g_output,'\\');const size_t prefix=slash?(size_t)(slash-g_output)+1:0;
        if(prefix<sizeof path-64){memcpy(path,g_output,prefix);snprintf(path+prefix,sizeof path-prefix,"%s-texture%d.raw",g_scenario,c);FILE* out=fopen(path,"wb");if(out){fwrite(pixels,1,640*480,out);fclose(out);}}
    }
    memcpy((void*)0x484280,edges,5760);memcpy((void*)0x482bf8,edges+5760,5760);memcpy((void*)0x4c5d30,uv_saved,32);
    RD32(0x4c5d20)=prior_texture;RD32(0x4c5d50)=prior_output;
    for(int i=0;i<12;++i)RD32(addresses[i])=saved[i];
    free(edges);free(texture);free(pixels);
}

static uint32_t cel_word(const unsigned char* b,size_t p){uint32_t v;memcpy(&v,b+p,4);return v;}
static int cel_dimension(uint32_t value){return value&0x80000000u?(int)(1u<<(value&31)):value&511;}
static void capture_cels(void) {
    const char* files[6]={"GLOBAL.RSC","COURSES\\CANYN.CRS","COURSES\\CITY.CRS","COURSES\\HIWAY.CRS","COURSES\\MEDLY.CRS","COURSES\\NAPA.CRS"};
    unsigned char dimension_table[4096];memcpy(dimension_table,(void*)0x4b6e70,sizeof dimension_table);
    ((void (__cdecl*)(void))0x44fb10)();
    char key[128];int count=0;
    for(int file=0;file<6;++file) {
        char source_path[256];snprintf(source_path,sizeof source_path,"D:\\ROADRASH\\DATA\\%s",files[file]);
        FILE* in=fopen(source_path,"rb");if(!in)continue;
        fseek(in,0,SEEK_END);long size=ftell(in);rewind(in);
        if(size<24 || size>10000000){fclose(in);continue;}
        unsigned char* bytes=malloc(size);if(!bytes){fclose(in);continue;}
        if(fread(bytes,1,size,in)!=(size_t)size){free(bytes);fclose(in);continue;}fclose(in);
        const uint32_t table=cel_word(bytes,16);
        if(table>(uint32_t)size || 16>(uint32_t)size-table){free(bytes);continue;}
        const uint32_t entries=cel_word(bytes,table+8);
        if(entries>((uint32_t)size-table-16)/32){free(bytes);continue;}
        for(uint32_t i=0;i<entries;++i) {
            const size_t row=table+16+i*32;
            if(cel_word(bytes,row)!=0x43454c20)continue;
            const uint32_t id=cel_word(bytes,row+4),offset=cel_word(bytes,row+8),extent=cel_word(bytes,row+12);
            if(offset>(uint32_t)size || extent>(uint32_t)size-offset || extent<12)continue;
            int* ccb=((int* (__fastcall*)(void*,void*))0x40b800)(bytes+offset,NULL);
            if(!ccb)continue;
            ++count;
            const uint32_t data=(uint32_t)ccb[2];
#define CEL_LOG(label,value) do {snprintf(key,sizeof key,"cel.%d.%u.%s",file,id,label);rr_log(key,value);}while(0)
            CEL_LOG("width",cel_dimension(ccb[15]));CEL_LOG("height",cel_dimension(ccb[16]));CEL_LOG("flags",(uint32_t)ccb[0]);
            const uint32_t magic=readable(data,4)?cel_word((void*)(uintptr_t)data,0):0;
            CEL_LOG("magic",magic);
            if(magic==0xbc614e && readable(data,56)) {
                const unsigned char* body=(void*)(uintptr_t)data;
                const uint32_t width=cel_word(body,32),height=cel_word(body,36),levels=cel_word(body,40);
                CEL_LOG("texture_flags",cel_word(body,12));CEL_LOG("levels",levels);
                for(int bound=0;bound<4;++bound){short value;memcpy(&value,body+4+bound*2,2);snprintf(key,sizeof key,"cel.%d.%u.bound.%d",file,id,bound);rr_log(key,value);}
                if(width && width<=256 && height && height<=512 && levels<=9) for(uint32_t level=0;level<levels;++level) {
                    const uint32_t pointer=cel_word(body,56+12*level);
                    const size_t pixels=(width>>level)*((height>>level)?height>>level:1);
                    if(!readable(pointer,pixels))continue;
                    char output_path[1024];const char* slash=strrchr(g_output,'\\');const size_t prefix=slash?(size_t)(slash-g_output)+1:0;
                    memcpy(output_path,g_output,prefix);snprintf(output_path+prefix,sizeof output_path-prefix,"%s-cel%d-%u-mip%u.raw",g_scenario,file,id,level);
                    FILE* out=fopen(output_path,"wb");if(out){fwrite((void*)(uintptr_t)pointer,1,pixels,out);fclose(out);}
                }
            }
#undef CEL_LOG
        }
        free(bytes);
    }
    rr_log("cel.count",count);memcpy((void*)0x4b6e70,dimension_table,sizeof dimension_table);
}

static void capture_node_render(void) {
    const uint32_t addresses[]={0x4b7eb0,0x4b7eb4,0x4b7eb8,0x4b7ebc,0x4b7ec0,0x4b7ec4,0x4c5d88,0x4c5d60,0x4c5d64,0x4c5d68,0x4c5d6c,0x484278,0x4c5d20,0x4c5d50,0x466680,0x481fe0,0x481fa8,0x481ff4,0x475648,0x481fb0,0x481fbc};
    int saved[sizeof addresses/sizeof addresses[0]];for(unsigned i=0;i<sizeof addresses/sizeof addresses[0];++i)saved[i]=RD32(addresses[i]);
    const unsigned char locked=RD8(0x475620);RD8(0x475620)=1;RD32(0x466680)=1;RD32(0x481fb0)=0x10;RD32(0x481fbc)=0x10;
    unsigned char* pixels=malloc(640*480);unsigned char* texture=malloc(1344);unsigned char* edges=malloc(11520);unsigned char uv_saved[32];
    if(!pixels || !texture || !edges){free(pixels);free(texture);free(edges);goto restore;}
    memcpy(edges,(void*)0x484280,5760);memcpy(edges+5760,(void*)0x482bf8,5760);memcpy(uv_saved,(void*)0x4c5d30,32);
    for(int level=0;level<3;++level){const int offset=level==0?0:level==1?1024:1280,size=1024>>(level*2);for(int i=0;i<size;++i)texture[offset+i]=(unsigned char)(i*37+i/7+level*53);}
    const int masks[3]={16,48,112},flags[6]={0,2,4,8,16,32};char key[96];
    for(int c=0;c<32;++c) {
        const int high=c%2,width=high?640:320,height=high?480:240;
        int source[12][42]={{0}},resource[12][32]={{0}},legacy=0;
        RD32(0x481fa8)=high?15:16;RD32(0x481ff4)=high?(int)0xfffffffeu:-1;RD32(0x475648)=masks[c%3];
        RD32(0x4b7eb0)=0;RD32(0x4b7eb4)=0;RD32(0x4b7eb8)=width;RD32(0x4b7ebc)=height-1;RD32(0x4b7ec0)=(int)(uintptr_t)pixels;RD32(0x4b7ec4)=width;
        memset(pixels,125,width*height);
        for(int n=0;n<12;++n) {
            source[n][1]=n<11?(int)(uintptr_t)source[n+1]:0;source[n][2]=(int)(uintptr_t)(n%4==0?&legacy:resource[n]);source[n][3]=n==11?0x40000000:0;
            resource[n][0]=0xbc614e;resource[n][2]=31|(31<<16);resource[n][3]=flags[(c+n)%6];resource[n][8]=32;resource[n][9]=32;resource[n][10]=3;resource[n][11]=196+n;
            resource[n][14]=(int)(uintptr_t)texture;resource[n][17]=(int)(uintptr_t)(texture+1024);resource[n][20]=(int)(uintptr_t)(texture+1280);
            unsigned char* bytes=(void*)source[n];bytes[0x64]=(c+n)%2;bytes[0x65]=(c+n)%3==0;bytes[0x66]=n%4==0;bytes[0x67]=20+n;
            const int left=(n*37+c*13)%420-60,top=(n*29+c*17)%320-40,right=left+20+(n*23+c*7)%180,bottom=top+10+(n*13+c*3)%100;
            const int points[8]={left,top,right,top+((n+c)%3),right-3,bottom,left+5,bottom};
            for(int i=0;i<8;++i)source[n][0x44/4+i]=points[i]*65536+((c+n+i)*1777)%65536;
            snprintf(key,sizeof key,"nodes.%d.%d.flat",c,n);rr_log(key,n%4==0?20+n:-1);
            snprintf(key,sizeof key,"nodes.%d.%d.flags",c,n);rr_log(key,resource[n][3]);
            snprintf(key,sizeof key,"nodes.%d.%d.index",c,n);rr_log(key,resource[n][11]);
            snprintf(key,sizeof key,"nodes.%d.%d.flip_u",c,n);rr_log(key,bytes[0x64]);snprintf(key,sizeof key,"nodes.%d.%d.flip_v",c,n);rr_log(key,bytes[0x65]);
            for(int i=0;i<8;++i){snprintf(key,sizeof key,"nodes.%d.%d.point.%d",c,n,i);rr_log(key,source[n][0x44/4+i]);}
        }
        ((int (__fastcall*)(void*))0x40ff40)(source[0]);
        snprintf(key,sizeof key,"nodes.%d.mask",c);rr_log(key,masks[c%3]);snprintf(key,sizeof key,"nodes.%d.high",c);rr_log(key,high);
        char path[1024];const char* slash=strrchr(g_output,'\\');const size_t prefix=slash?(size_t)(slash-g_output)+1:0;memcpy(path,g_output,prefix);snprintf(path+prefix,sizeof path-prefix,"%s-nodes%d.raw",g_scenario,c);
        FILE* out=fopen(path,"wb");if(out){fwrite(pixels,1,width*height,out);fclose(out);}
    }
    memcpy((void*)0x484280,edges,5760);memcpy((void*)0x482bf8,edges+5760,5760);memcpy((void*)0x4c5d30,uv_saved,32);
    free(edges);free(texture);free(pixels);
restore:
    RD8(0x475620)=locked;for(unsigned i=0;i<sizeof addresses/sizeof addresses[0];++i)RD32(addresses[i])=saved[i];
}

static void capture_road_panels(void) {
    const uint32_t addresses[]={0x49e660,0x49e668,0x4b7e74,0x46667c,0x4c3cc0,0x4c3cb0,0x4c3cd0,0x4c3cd4,0x4c3cd8,0x4c3ce0,0x4c3ce4,0x4c3ce8};
    int saved[12];for(int i=0;i<12;++i)saved[i]=RD32(addresses[i]);
    int templates[8][17]={{0}},output[16][42],current[65],previous[65],terrain[96],old_terrain[96];
    for(int i=0;i<8;++i){templates[i][2]=i;templates[i][15]=5;templates[i][16]=5;}
    RD32(0x4b7e74)=(int)(uintptr_t)templates[0];RD32(0x4c3cc0)=(int)(uintptr_t)templates[0];RD32(0x4c3cb0)=(int)(uintptr_t)templates[1];
    for(int i=0;i<3;++i){RD32(0x4c3cd0+i*4)=(int)(uintptr_t)templates[2+i];RD32(0x4c3ce0+i*4)=(int)(uintptr_t)templates[5+i];}
    const uint32_t draw_addresses[]={0x4b7eb0,0x4b7eb4,0x4b7eb8,0x4b7ebc,0x4b7ec0,0x4b7ec4,0x4c5d88,0x4c5d60,0x4c5d64,0x4c5d68,0x4c5d6c,0x484278,0x4c5d20,0x4c5d50,0x466680,0x481fe0,0x481fa8,0x481ff4,0x475648};
    int draw_saved[19];for(int i=0;i<19;++i)draw_saved[i]=RD32(draw_addresses[i]);
    unsigned char* pixels=malloc(640*480);unsigned char* edges=malloc(11520);unsigned char uv_saved[32],dimension_table[4096];
    const unsigned char locked=RD8(0x475620);RD8(0x475620)=1;RD32(0x466680)=1;
    if(edges){memcpy(edges,(void*)0x484280,5760);memcpy(edges+5760,(void*)0x482bf8,5760);}memcpy(uv_saved,(void*)0x4c5d30,32);
    memcpy(dimension_table,(void*)0x4b6e70,4096);((void (__cdecl*)(void))0x44fb10)();
    unsigned char* global=NULL;int* real_ccb[8]={0};
    FILE* input=fopen("D:\\ROADRASH\\DATA\\GLOBAL.RSC","rb");
    if(input){fseek(input,0,SEEK_END);long size=ftell(input);rewind(input);if(size>0 && size<10000000){global=malloc(size);if(global && fread(global,1,size,input)==(size_t)size){
        const uint32_t table=cel_word(global,16),entries=cel_word(global,table+8);
        for(int slot=0;slot<8;++slot){const uint32_t destination=slot==0?0x4c3cc0:slot==1?0x4c3cb0:slot<5?0x4c3cd0+(slot-2)*4:0x4c3ce0+(slot-5)*4;
            uint32_t id=0;for(int* descriptor=(void*)0x46dfb0;descriptor[0];descriptor+=5)if((uint32_t)descriptor[0]==destination)id=descriptor[2]+1;
            if(slot==3){real_ccb[3]=real_ccb[2];continue;}
            for(uint32_t row=0;row<entries;++row){const unsigned char* entry=global+table+16+row*32;if(cel_word(entry,0)==0x43454c20 && cel_word(entry,4)==id)real_ccb[slot]=((int* (__fastcall*)(void*,void*))0x40b800)(global+cel_word(entry,8),NULL);}
        }
    }}fclose(input);}
    const int marking_flags[8]={0,8,16,32,64,24,80,255};char key[128];
    for(int c=0;c<128;++c) {
        memset(current,0,sizeof current);memset(previous,0,sizeof previous);memset(terrain,0,sizeof terrain);memset(old_terrain,0,sizeof old_terrain);memset(output,0,sizeof output);
        unsigned char* t=(void*)terrain;unsigned char* old=(void*)old_terrain;
        current[0]=(int)(uintptr_t)previous;current[3]=(int)(uintptr_t)terrain;previous[3]=(int)(uintptr_t)old_terrain;
        current[0x40]=c%17!=0;current[0xc]=((c*71)%900-400)*65536;current[0x10]=((c*53)%900-400)*65536;
        current[10]=((c*17)%260-20)*65536+c*1223;current[14]=((c*23)%260-20)*65536+c*1777;
        previous[9]=((c*37)%400-100)*65536;current[9]=((c*43)%400-100)*65536;
        terrain[0x30/4]=(c%6)*256+((c*113)%256)-128;terrain[0x2c/4]=(c%4)*256+((c*177)%256)-128;
        t[0x34]=c%5;t[0x35]=(c/5)%5;t[0x36]=c%3==0?32:0;t[0x37]=marking_flags[c%8];t[0x3c]=c%5;old[0x3c]=(c/5)%5;
#define PANEL_LOG(label,value) do{snprintf(key,sizeof key,"panels.%d.%s",c,label);rr_log(key,value);}while(0)
        PANEL_LOG("first_left",current[0xc]);PANEL_LOG("second_left",current[0x10]);PANEL_LOG("first_y",current[10]);PANEL_LOG("second_y",current[14]);
        PANEL_LOG("first_step",previous[9]);PANEL_LOG("second_step",current[9]);PANEL_LOG("left_q8",terrain[0x30/4]);PANEL_LOG("right_q8",terrain[0x2c/4]);
        PANEL_LOG("left_count",t[0x34]);PANEL_LOG("right_count",t[0x35]);PANEL_LOG("surface",t[0x36]);PANEL_LOG("marking",t[0x37]);
        PANEL_LOG("type",t[0x3c]);PANEL_LOG("next_type",old[0x3c]);PANEL_LOG("visible",current[0x40]);
        RD32(0x46667c)=c%2?640:320;PANEL_LOG("width",RD32(0x46667c));
        RD32(0x49e660)=(int)(uintptr_t)output;RD32(0x49e668)=(int)(uintptr_t)(output+15);
        ((void (__fastcall*)(int*))0x43c170)(current);
        const int count=((uintptr_t)RD32(0x49e660)-(uintptr_t)output)/168;PANEL_LOG("count",count);
        for(int n=0;n<count && n<16;++n){
            snprintf(key,sizeof key,"panels.%d.%d.material",c,n);rr_log(key,output[n][2]);snprintf(key,sizeof key,"panels.%d.%d.pre0",c,n);rr_log(key,(uint32_t)output[n][12]);
            const unsigned char* bytes=(void*)output[n];snprintf(key,sizeof key,"panels.%d.%d.flip_u",c,n);rr_log(key,bytes[100]);snprintf(key,sizeof key,"panels.%d.%d.flip_v",c,n);rr_log(key,bytes[101]);
            for(int i=0;i<8;++i){snprintf(key,sizeof key,"panels.%d.%d.point.%d",c,n,i);rr_log(key,output[n][17+i]);}
        }
        const int high=c%2,width=high?640:320,height=high?480:240,mask=c%3==0?16:c%3==1?48:112;
        PANEL_LOG("draw_high",high);PANEL_LOG("draw_mask",mask);
        if(pixels && edges){
            for(int n=0;n<count && n<16;++n){const int slot=output[n][2];output[n][2]=real_ccb[slot]?real_ccb[slot][2]:0;output[n][1]=n+1<count?(int)(uintptr_t)output[n+1]:0;output[n][3]=n+1==count?0x40000000:0;}
            RD32(0x481fa8)=high?15:16;RD32(0x481ff4)=high?(int)0xfffffffeu:-1;RD32(0x475648)=mask;
            RD32(0x4b7eb0)=0;RD32(0x4b7eb4)=0;RD32(0x4b7eb8)=width;RD32(0x4b7ebc)=height-1;RD32(0x4b7ec0)=(int)(uintptr_t)pixels;RD32(0x4b7ec4)=width;
            memset(pixels,125,width*height);if(count)((void (__fastcall*)(void*))0x40ff40)(output);
            char path[1024];const char* slash=strrchr(g_output,'\\');const size_t prefix=slash?(size_t)(slash-g_output)+1:0;memcpy(path,g_output,prefix);snprintf(path+prefix,sizeof path-prefix,"%s-panels%d.raw",g_scenario,c);
            FILE* out=fopen(path,"wb");if(out){fwrite(pixels,1,width*height,out);fclose(out);}
        }
#undef PANEL_LOG
    }
    if(edges){memcpy((void*)0x484280,edges,5760);memcpy((void*)0x482bf8,edges+5760,5760);}memcpy((void*)0x4c5d30,uv_saved,32);memcpy((void*)0x4b6e70,dimension_table,4096);
    free(global);free(edges);free(pixels);RD8(0x475620)=locked;
    for(int i=0;i<19;++i)RD32(draw_addresses[i])=draw_saved[i];
    for(int i=0;i<12;++i)RD32(addresses[i])=saved[i];
}

static void dump_frame(uint32_t site) {
    char key[96];
    fprintf(g_out, "frame\t%u\n", g_frame);
    rr_log("site", site);
    rr_log("course", RD8(0x00467a6d));
    rr_log("view.high", RD8(0x00475380));
    rr_log("camera.position", RD32(0x004bc7e4));
    rr_log("camera.x", RD32(0x004bc7d8));
    rr_log("camera.height", RD32(0x004bc7dc));
    rr_log("camera.half", RD8(0x00473498));
    rr_log("view.coordinate_shift", RD32(0x00481fa8));
    rr_log("view.coordinate_mask", (uint32_t)RD32(0x00481ff4));
    if (g_frame==0) dump_raster();
    if (g_pixel_written) {
        rr_log("pixels.file",g_frame);
        rr_log("pixels.width",g_pixel_width); rr_log("pixels.height",g_pixel_height);
        rr_log("pixels.pitch",g_pixel_pitch);
        for (int i=0;i<256;++i) {
            const uintptr_t p=0x0049e138+i*4;
            snprintf(key,sizeof key,"palette.%d",i);
            rr_log(key,((uint32_t)RD8(p)<<16)|((uint32_t)RD8(p+1)<<8)|RD8(p+2));
        }
    }
    for (int s = 0; s < 33; ++s) {
        uintptr_t entry = 0x004bc800 + (uintptr_t)s * 548;
        int kind = RD32(entry + 4);
        snprintf(key, sizeof key, "slice.%d.kind", s); rr_log(key, kind);
        snprintf(key, sizeof key, "slice.%d.sample", s); rr_log(key, RD32(entry + 8));
        for (int r = 0; r < kind && r < 2; ++r) {
            uintptr_t p = (uint32_t)RD32(entry + (5 + r) * 4);
            if (!readable(p, 0x104)) continue;
            static const struct { const char* name; int off, byte; } fields[] = {
                {"depth",0x18,0},{"scale",0x20,0},{"x",0x10,0},{"y",0x14,0},{"centre",0x48,0},
                {"screen_y",0x4c,0},{"left",0x50,0},{"right",0x58,0},{"lane_left",0x40,0},{"lane_right",0x44,0},
                {"shoulder_left",0x60,0},{"shoulder_right",0x68,0},{"height_left_row",0x54,0},
                {"height_right_row",0x5c,0},{"visible",0x100,1}
            };
            for (unsigned f = 0; f < sizeof fields / sizeof fields[0]; ++f) {
                snprintf(key, sizeof key, "slice.%d.road.%d.%s", s, r, fields[f].name);
                rr_log(key, fields[f].byte ? RD8(p + fields[f].off) : RD32(p + fields[f].off));
            }
            uintptr_t sub = (uint32_t)RD32(p + 0x0c);
            snprintf(key, sizeof key, "slice.%d.road.%d.type", s, r);
            rr_log(key, readable(sub, 0x3d) ? RD8(sub + 0x3c) : -1);
            if (readable(sub,0x164)) {
                for (int offset=0xf0;offset<=0x160;offset+=4) {
                    snprintf(key,sizeof key,"slice.%d.road.%d.sub.%d",s,r,offset);
                    rr_log(key,RD32(sub+offset));
                }
            }
        }
    }
    fflush(g_out);
}

#define FRAME_HOOK(n, site) \
    static void __cdecl hook_frame_##n(void) { \
        orig_frame(); \
        if (g_out) dump_frame(site); \
        ++g_frame; \
        if (g_quit_after > 0 && g_frame >= (uint32_t)g_quit_after) { \
            if (g_out) { fputs("end\n", g_out); fclose(g_out); g_out = NULL; } \
            ExitProcess(0); \
        } \
    }
FRAME_HOOK(0, 0x004163fa)
FRAME_HOOK(1, 0x00416406)
FRAME_HOOK(2, 0x0041656d)
FRAME_HOOK(3, 0x00443249)

static int rr_install_hooks(void) {
    char buf[32];
    if (GetEnvironmentVariableA("RR_ORACLE_FRAMES", buf, sizeof buf)) g_quit_after = atoi(buf);
    if (GetEnvironmentVariableA("RR_ORACLE_PIXELS",buf,sizeof buf)) g_pixels=atoi(buf)!=0;
    env("RR_ORACLE_RASTER",buf,sizeof buf,"0");g_raster=atoi(buf);
    if (GetEnvironmentVariableA("RR_ORACLE_SHOT_EVERY",buf,sizeof buf) && atoi(buf)>0) g_pixel_every=atoi(buf);
    static const struct { uint32_t site; void* hook; uint8_t bytes[5]; } calls[] = {
        {0x004163fa, hook_frame_0, {0xe8,0xf1,0xd5,0x02,0}},
        {0x00416406, hook_frame_1, {0xe8,0xe5,0xd5,0x02,0}},
        {0x0041656d, hook_frame_2, {0xe8,0x7e,0xd4,0x02,0}},
        {0x00443249, hook_frame_3, {0xe8,0xa2,0x07,0,0}},
        {0x004439f5, hook_draw, {0xe8,0x26,0xca,0,0}}
    };
    const unsigned count=sizeof calls/sizeof calls[0];
    // Validate ALL sites before changing ANY. A foreign binary is never patched.
    for (unsigned i = 0; i < count; ++i) {
        if (memcmp((void*)(uintptr_t)calls[i].site, calls[i].bytes, 5)) {
            if (g_out) fprintf(g_out, "# patch verification failed at %08x\n", calls[i].site);
            return 0;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        const uint32_t target=calls[i].site==0x004439f5 ? 0x00450420 : 0x004439f0;
        if (!rr_patch_call((uint8_t*)(uintptr_t)calls[i].site, target, calls[i].hook)) {
            // Restore any earlier call sites on a protection failure.
            for (unsigned j = 0; j < i; ++j) {
                DWORD old;
                if (VirtualProtect((void*)(uintptr_t)calls[j].site, 5, PAGE_EXECUTE_READWRITE, &old)) {
                    memcpy((void*)(uintptr_t)calls[j].site, calls[j].bytes, 5);
                    VirtualProtect((void*)(uintptr_t)calls[j].site, 5, old, &old);
                    FlushInstructionCache(GetCurrentProcess(), (void*)(uintptr_t)calls[j].site, 5);
                }
            }
            if (g_out) fprintf(g_out, "# patch installation failed at %08x\n", calls[i].site);
            return 0;
        }
    }
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)inst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        env("RR_ORACLE_OUT", path, sizeof path, "C:\\oracle.tsv");
        memcpy(g_output,path,sizeof path);
        env("RR_ORACLE_SCENARIO", g_scenario, sizeof g_scenario, "unnamed");
        g_out = fopen(path, "wb");
        if (g_out) {
            setvbuf(g_out, NULL, _IOFBF, 1 << 20);
            fprintf(g_out, "# rr-oracle 1\nexe\t73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2\nscenario\t%s\n", g_scenario);
        }
        HMODULE real = LoadLibraryA("AWEMAN_REAL.DLL");
        g_real2 = real ? (void*)GetProcAddress(real, MAKEINTRESOURCEA(2)) : NULL;
        if (!g_real2) {
            if (g_out) { fputs("# AWEMAN_REAL.DLL ordinal 2 not found\n", g_out); fclose(g_out); g_out = NULL; }
            return FALSE;
        }
        if (!rr_install_hooks()) {
            if (g_out) { fclose(g_out); g_out = NULL; }
            return FALSE;
        }
        if (g_raster==2 && g_out) {
            const int width=640,height=480;
            void* pixels=malloc((size_t)width*height);
            if (pixels) {
                int base=RD32(0x004b7ec0),pitch=RD32(0x004b7ec4);
                RD32(0x004b7ec0)=(int)(uintptr_t)pixels;RD32(0x004b7ec4)=width;
                capture_raster((uintptr_t)pixels,width,height,width);
                RD32(0x004b7ec0)=base;RD32(0x004b7ec4)=pitch;free(pixels);
                fputs("frame\t0\n",g_out);dump_raster();capture_view();capture_terrain();capture_hill_projection();capture_polygon();capture_texture_spans();capture_texture_quads();capture_cels();capture_node_render();capture_road_panels();fputs("end\n",g_out);
                fclose(g_out);g_out=NULL;
            }
        }
        if (g_out) fflush(g_out);
    } else if (reason == DLL_PROCESS_DETACH && g_out) {
        fputs("end\n", g_out);
        fclose(g_out);
        g_out = NULL;
    }
    return TRUE;
}
#endif
