#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Performance {
struct Distribution {
    size_t samples=0;
    double mean=0,p50=0,p95=0,maximum=0;
};
class Series {
    std::array<double,256> values{};
    size_t count=0,next=0;
public:
    void Add(double value) {
        if(!std::isfinite(value)||value<0)return;
        values[next]=value;next=(next+1)%values.size();count=(std::min)(count+1,values.size());
    }
    void Clear(){count=next=0;}
    Distribution Summary() const {
        Distribution out;out.samples=count;if(!count)return out;
        auto sorted=values;std::sort(sorted.begin(),sorted.begin()+count);
        for(size_t i=0;i<count;++i)out.mean+=sorted[i];out.mean/=count;
        out.p50=sorted[(count-1)/2];out.p95=sorted[(95*count+99)/100-1];out.maximum=sorted[count-1];return out;
    }
};
struct Snapshot {
    bool running=false,enabled=false,heapAvailable=false,deviceMeasurement=false,fpsAvailable=false;
    std::string source="Windows/Linux editor preview",device;
    uint64_t frames=0,redrawPixels=0,inputBytes=0,framebufferBytes=0;
    bool ramAvailable=false,firmwareAvailable=false;
    uint64_t ramPeak=0,firmwareBytes=0;
    size_t heapUsed=0,heapPeak=0,largestFree=0,totalScreens=0,loadedScreens=0,loadedObjects=0;
    double elapsedSeconds=0,fps=0,loadMs=0,lastScreenLoadMs=0;
    Distribution frame,render,flush,key,prediction;
};
struct Budget {
    std::string name="Custom";
    uint64_t ramBytes=0,flashBytes=0;
    double frameMs=33.333,predictionMs=10;
};
inline const std::vector<Budget>& BudgetPresets() {
    // Application allocations, not advertised board capacities. Users should
    // reserve memory for their own firmware, OS, drivers and display buffers.
    static const std::vector<Budget> presets={
        {"Custom",0,0,33.333,10},
        {"ESP32 application",256u*1024u,4u*1024u*1024u,33.333,10},
        {"Raspberry Pi / Linux application",64u*1024u*1024u,0,16.667,5},
        {"Windows preview",256u*1024u*1024u,0,16.667,5}};
    return presets;
}
}
