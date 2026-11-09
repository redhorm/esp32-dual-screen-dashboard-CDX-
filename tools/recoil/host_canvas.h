#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

// Host-only adapter; the device uses the same renderer with LGFX_Sprite.
class HostCanvas {
    int w_,h_;
public:
    std::vector<uint16_t> pixels;
    int clipped=0;
    HostCanvas(int w,int h):w_(w),h_(h),pixels(w*h) {}
    int width() const {return w_;}
    int height() const {return h_;}
    uint16_t at(int x,int y) const {return pixels[y*w_+x];}
    void fillScreen(uint16_t c) {std::fill(pixels.begin(),pixels.end(),c);}
    void put(int x,int y,uint16_t c) {
        if(x<0||x>=w_||y<0||y>=h_) {++clipped;return;}
        pixels[y*w_+x]=c;
    }
    void fillRect(int x,int y,int w,int h,uint16_t c) {
        for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)put(xx,yy,c);
    }
    void drawLine(int x,int y,int bx,int by,uint16_t c) {
        int dx=std::abs(bx-x), sx=x<bx?1:-1;
        int dy=-std::abs(by-y), sy=y<by?1:-1, err=dx+dy;
        for(;;) {
            put(x,y,c);if(x==bx&&y==by)break;
            const int e=2*err;
            if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}
        }
    }
    bool save(const char* path) const {
        std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w_<<" "<<h_<<"\n255\n";
        for(auto p:pixels) {
            const unsigned char rgb[3]={static_cast<unsigned char>(((p>>11)&31)*255/31),
              static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};
            out.write(reinterpret_cast<const char*>(rgb),3);
        }
        return static_cast<bool>(out);
    }
};
