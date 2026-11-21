#pragma once
#include "Metrics.h"
#include "../../3rdParty/JSON/json.hpp"
#include <stdexcept>
namespace Performance {
using json=nlohmann::json;
inline Distribution Samples(const json& data,const char* key) {
    const auto values=data.value(key,json::array());
    if(!values.is_array()||values.size()>4096)throw std::invalid_argument(std::string(key)+" must be an array of at most 4096 measured milliseconds");
    Series series;for(const auto& value:values){if(!value.is_number()||!std::isfinite(value.get<double>())||value.get<double>()<0)throw std::invalid_argument("Measurements must be finite nonnegative numbers");series.Add(value.get<double>());}return series.Summary();
}
inline uint64_t Counter(const json& data,const char* key,uint64_t fallback=0) {
    if(!data.contains(key))return fallback;
    const auto& value=data.at(key);if(!value.is_number_integer()||(!value.is_number_unsigned()&&value.get<int64_t>()<0))throw std::invalid_argument(std::string(key)+" must be a nonnegative integer");return value.get<uint64_t>();
}
inline Snapshot DeviceSnapshot(const json& data) {
    if(!data.is_object()||data.value("format",std::string{})!="lvgl-performance-sample"||data.value("version",0)!=1)
        throw std::invalid_argument("Choose an lvgl-performance-sample version 1 JSON file");
    Snapshot out;out.deviceMeasurement=true;out.enabled=true;out.source="Imported device measurements";out.device=data.value("device",std::string{});
    if(out.device.empty()||out.device.size()>120)throw std::invalid_argument("Device measurements need a device name up to 120 characters");
    out.frame=Samples(data,"frameMs");out.render=Samples(data,"renderMs");out.flush=Samples(data,"flushMs");out.key=Samples(data,"keyMs");out.prediction=Samples(data,"predictionMs");
    out.frames=Counter(data,"frames",out.frame.samples);out.redrawPixels=Counter(data,"redrawPixels");out.inputBytes=Counter(data,"inputBytes");
    out.elapsedSeconds=data.value("elapsedSeconds",0.0);if(!std::isfinite(out.elapsedSeconds)||out.elapsedSeconds<0)throw std::invalid_argument("Elapsed time must be a nonnegative number");
    out.fpsAvailable=out.elapsedSeconds>0;out.fps=out.fpsAvailable?double(out.frames)/out.elapsedSeconds:0;
    if(!std::isfinite(out.fps))throw std::invalid_argument("Elapsed time is too small for the recorded frame count");
    out.ramAvailable=data.contains("ramPeakBytes");out.ramPeak=Counter(data,"ramPeakBytes");
    out.firmwareAvailable=data.contains("firmwareBytes");out.firmwareBytes=Counter(data,"firmwareBytes");
    out.loadedScreens=Counter(data,"loadedScreens");out.totalScreens=Counter(data,"totalScreens");out.loadedObjects=Counter(data,"loadedObjects");
    out.loadMs=data.value("loadMs",0.0);if(!std::isfinite(out.loadMs)||out.loadMs<0)throw std::invalid_argument("Load time must be nonnegative");return out;
}
inline json DistributionJson(const Distribution& d){return {{"samples",d.samples},{"meanMs",d.mean},{"p50Ms",d.p50},{"p95Ms",d.p95},{"maxMs",d.maximum}};}
inline json Report(const Snapshot& s) {
    return {{"format","lvgl-performance-report"},{"version",1},{"source",s.source},{"device",s.device},{"deviceMeasurement",s.deviceMeasurement},
        {"frames",s.frames},{"fpsAvailable",s.fpsAvailable},{"fps",s.fps},{"elapsedSeconds",s.elapsedSeconds},{"redrawPixels",s.redrawPixels},
        {"loadMs",s.loadMs},{"lastScreenLoadMs",s.lastScreenLoadMs},{"inputBytes",s.inputBytes},{"framebufferBytes",s.framebufferBytes},
        {"loadedScreens",s.loadedScreens},{"totalScreens",s.totalScreens},{"loadedObjects",s.loadedObjects},
        {"lvglHeapAvailable",s.heapAvailable},{"lvglHeapUsed",s.heapUsed},{"lvglHeapPeak",s.heapPeak},{"largestFreeBlock",s.largestFree},
        {"ramAvailable",s.ramAvailable},{"ramPeakBytes",s.ramPeak},{"firmwareAvailable",s.firmwareAvailable},{"firmwareBytes",s.firmwareBytes},
        {"frame",DistributionJson(s.frame)},{"render",DistributionJson(s.render)},{"flush",DistributionJson(s.flush)},
        {"key",DistributionJson(s.key)},{"prediction",DistributionJson(s.prediction)}};
}
}
