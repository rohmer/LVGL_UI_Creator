#include "catch.hpp"
#include "../../../UICreator/Performance/Telemetry.h"
#include <limits>
TEST_CASE("Performance samples are bounded and percentiles use measured values", "[runtime][performance]") {
    Performance::Series series;for(unsigned i=0;i<300;++i)series.Add(double(i));
    series.Add(-1);series.Add(std::numeric_limits<double>::quiet_NaN());series.Add(std::numeric_limits<double>::infinity());
    const auto summary=series.Summary();CHECK(summary.samples==256);CHECK(summary.mean==171.5);CHECK(summary.p50==171);CHECK(summary.p95==287);CHECK(summary.maximum==299);
    series.Clear();CHECK(series.Summary().samples==0);
}
TEST_CASE("Device telemetry distinguishes unknown measurements and rejects invented numeric data", "[runtime][performance]") {
    using json=nlohmann::json;
    json data={{"format","lvgl-performance-sample"},{"version",1},{"device","ESP32 test rig"},
        {"frameMs",{10,12,14}},{"frames",3},{"elapsedSeconds",0.1},{"ramPeakBytes",120000},{"firmwareBytes",600000}};
    const auto snapshot=Performance::DeviceSnapshot(data);CHECK(snapshot.deviceMeasurement);CHECK(snapshot.fpsAvailable);CHECK(snapshot.fps==30);
    CHECK(snapshot.frame.samples==3);CHECK(snapshot.frame.p95==14);CHECK(snapshot.ramAvailable);CHECK(snapshot.ramPeak==120000);CHECK(snapshot.firmwareAvailable);
    CHECK(snapshot.prediction.samples==0);CHECK_FALSE(snapshot.heapAvailable);
    auto report=Performance::Report(snapshot);CHECK(report["source"]=="Imported device measurements");CHECK(report["frame"]["p95Ms"]==14);
    data.erase("elapsedSeconds");data.erase("ramPeakBytes");auto unknown=Performance::DeviceSnapshot(data);CHECK_FALSE(unknown.fpsAvailable);CHECK_FALSE(unknown.ramAvailable);
    data["frameMs"]={1,-2};CHECK_THROWS(Performance::DeviceSnapshot(data));data["frameMs"]={1};data["ramPeakBytes"]=-1;CHECK_THROWS(Performance::DeviceSnapshot(data));
    data.erase("ramPeakBytes");data["frameMs"]=json::array();for(int i=0;i<4097;++i)data["frameMs"].push_back(1);CHECK_THROWS(Performance::DeviceSnapshot(data));
}
