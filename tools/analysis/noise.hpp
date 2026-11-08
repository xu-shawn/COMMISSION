#pragma once
// Minimal, independent sampler of Minecraft's continentalness and shift noises (1.18+ / 26.x), used by the
// analysis tools: xoroshiro128++ seeding, the octave salts, Perlin octaves with Minecraft's gradient table and
// the two-stack normal noise (the second stack sampled at 337/331 of the coordinates). Bit-exact to the game.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace standalone {
inline uint64_t mix(uint64_t x) {
    x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
    x=(x^(x>>27))*0x94d049bb133111ebULL;
    return x^(x>>31);
}
struct Random {
    uint64_t a,b;
    explicit Random(uint64_t seed) {
        seed ^= 0x6a09e667f3bcc909ULL;
        a=mix(seed); b=mix(seed+0x9e3779b97f4a7c15ULL);
    }
    Random(uint64_t a_,uint64_t b_):a(a_),b(b_){}
    uint64_t next() {
        uint64_t result=std::rotl(a+b,17)+a;
        b^=a; a=std::rotl(a,49)^b^(b<<21); b=std::rotl(b,28);
        return result;
    }
    double uniform() { return (next()>>11)*0x1p-53; }
    uint32_t bounded(uint32_t n) {
        uint64_t r=uint64_t(uint32_t(next()))*n;
        if(uint32_t(r)<n) while(uint32_t(r)<uint32_t(-n)%n)
            r=uint64_t(uint32_t(next()))*n;
        return r>>32;
    }
};
inline double fade(double t) { return t*t*t*(t*(t*6.-15.)+10.); }
template<class T> inline T interpolate(T t,T a,T b) { return a+t*(b-a); }
inline double gradient(unsigned h,double x,double y,double z) {
    // Minecraft's 16 gradient vectors, including its four duplicates. Components are 0/+-1, so
    // the products are exact and adding the zero term never changes a nonzero two-term sum:
    // branch-free but bit-identical to the two-term forms.
    static constexpr double gx[16]={1,-1,1,-1,1,-1,1,-1,0,0,0,0,1,0,-1,0};
    static constexpr double gy[16]={1,1,-1,-1,0,0,0,0,1,-1,1,-1,1,-1,1,-1};
    static constexpr double gz[16]={0,0,0,0,1,1,-1,-1,1,1,-1,-1,0,1,0,-1};
    h&=15;
    return gx[h]*x+gy[h]*y+gz[h]*z;
}
struct Perlin {
    std::array<uint8_t,256> permutation;
    double ox,oy,oz,frequency,weight;
    double fy,vy;
    int iy;
    void init(Random r,double f,double w) {
        ox=r.uniform()*256; oy=r.uniform()*256; oz=r.uniform()*256;
        frequency=f; weight=w;
        for(int i=0;i<256;i++) permutation[i]=i;
        for(int i=0;i<256;i++) std::swap(permutation[i],permutation[i+r.bounded(256-i)]);
        iy=int(std::floor(oy)); fy=oy-std::floor(oy); vy=fade(fy);
    }
    double sample(double x,double y,double z) const {
        x+=ox; z+=oz;
        double bx=std::floor(x),bz=std::floor(z);
        int ix=int(bx),iz=int(bz),jy=iy;
        x-=bx; z-=bz;
        double dy=fy,ty=vy;
        if(y!=0) { y+=oy; double by=std::floor(y); jy=int(by);dy=y-by;ty=fade(dy); }
        auto hash=[&](int dx,int dyy,int dz) {
            int a=permutation[(ix+dx)&255];
            int b=permutation[(a+jy+dyy)&255];
            return permutation[(b+iz+dz)&255];
        };
        double tx=fade(x),tz=fade(z);
        double zlo=interpolate(ty,
            interpolate(tx,gradient(hash(0,0,0),x,dy,z),gradient(hash(1,0,0),x-1,dy,z)),
            interpolate(tx,gradient(hash(0,1,0),x,dy-1,z),gradient(hash(1,1,0),x-1,dy-1,z)));
        double zhi=interpolate(ty,
            interpolate(tx,gradient(hash(0,0,1),x,dy,z-1),gradient(hash(1,0,1),x-1,dy,z-1)),
            interpolate(tx,gradient(hash(0,1,1),x,dy-1,z-1),gradient(hash(1,1,1),x-1,dy-1,z-1)));
        return interpolate(tz,zlo,zhi);
    }
};
// MD5-derived positional RNG salts for octave_-10 through octave_0.
inline constexpr uint64_t octave_salt[][2]={
 {0x36d326eed40efeb2ULL,0x5be9ce18223c636aULL},{0x082fe255f8be6631ULL,0x4e96119e22dedc81ULL},
 {0x0ef68ec68504005eULL,0x48b6bf93a2789640ULL},{0xf11268128982754fULL,0x257a1d670430b0aaULL},
 {0xe51c98ce7d1de664ULL,0x5f9478a733040c45ULL},{0x6d7b49e7e429850aULL,0x2e3063c622a24777ULL},
 {0xbd90d5377ba1b762ULL,0xc07317d419a7548dULL},{0x53d39c6752dac858ULL,0xbcd1c5a80ab65b3eULL},
 {0xb4a24d7a84e7677bULL,0x023ff9668e89b5c4ULL},{0xdffa22b534c5f608ULL,0xb9b67517d3665ca9ULL},
 {0xd50708086cef4d7cULL,0x6e1651ecc7f43309ULL}};
enum Channel { Temperature,Humidity,Continentalness,Erosion,Shift,Weirdness };
struct Config { uint64_t a,b; int first,len; double amplitude[9]; };
inline constexpr Config configs[]={
 {0x5c7e6b29735f0d7fULL,0xf7d86f1bbc734988ULL,-10,6,{1.5,0,1,0,0,0}},
 {0x81bb4d22e8dc168eULL,0xf1c8b4bea16303cdULL,-8,6,{1,1,0,0,0,0}},
 {0x83886c9d0ae3a662ULL,0xafa638a61b42e8adULL,-9,9,{1,1,2,2,2,1,1,1,1}},
 {0xd02491e6058f6fd8ULL,0x4792512c94c17a80ULL,-9,5,{1,1,0,1,1}},
 {0x080518cf6af25384ULL,0x3f3dfb40a54febd5ULL,-3,4,{1,1,1,0}},
 {0xefc8ef4d36102b34ULL,0x1beeeb324a0f24eaULL,-7,6,{1,2,1,0,0,0}}};
struct NormalNoise {
    Perlin octave[2][9];
    int n=0;
    double factor=0;
    void init(uint64_t a,uint64_t b,Channel channel) {
        const auto &c=configs[channel]; Random r(a^c.a,b^c.b);
        for(int side=0;side<2;side++) {
            uint64_t ra=r.next(),rb=r.next(); n=0;
            double f=std::ldexp(1.,c.first);
            double w=double(1<<(c.len-1))/((1<<c.len)-1);
            for(int i=0;i<c.len;i++,f*=2,w*=.5) if(c.amplitude[i]) {
                auto &salt=octave_salt[c.first+i+10];
                octave[side][n++].init(Random(ra^salt[0],rb^salt[1]),f,w*c.amplitude[i]);
            }
        }
        int lo=0,hi=c.len-1;
        while(!c.amplitude[lo]) lo++;
        while(!c.amplitude[hi]) hi--;
        int span=hi-lo+1;
        factor=(5.*span)/(3.*(span+1));
    }
    double sample(double x,double y,double z) const {
        double sums[2]={};
        for(int side=0;side<2;side++) {
            double scale=side?337./331.:1.;
            double px=x*scale,py=y*scale,pz=z*scale;
            for(int j=0;j<n;j++) {
                const auto &o=octave[side][j];
                sums[side]+=o.weight*o.sample(px*o.frequency,py*o.frequency,pz*o.frequency);
            }
        }
        return (sums[0]+sums[1])*factor;
    }
};

struct Sampler {
    uint64_t a, b;
    NormalNoise noise[6];
    bool ready[6] = {};
    explicit Sampler(uint64_t seed) { Random r(seed); a = r.next(); b = r.next(); }
    const NormalNoise &get(Channel c) {
        if (!ready[c]) { noise[c].init(a, b, c); ready[c] = true; }
        return noise[c];
    }
};
}
