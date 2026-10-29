// TextUtil.h - text helpers that need no SDK (STL only), so the tests build them.
#pragma once

#include <string>
#include <string_view>

namespace PickMyLoot {

// A slider's printf format from a text with a {value} ("{value} ms"): a '%' of the text is
// doubled first (so "{value}%" shows a percent sign), then the FIRST {value} becomes the
// conversion. The translation itself is never a format, and a later {value} stays literal
// text: ImGui passes one number, so a second conversion would read an argument that is not there.
inline std::string SliderFormat(const char* text, const char* conversion = "%d") {
    static constexpr std::string_view kValue = "{value}";
    const std::string_view t = text ? text : "";
    std::string out;
    bool placed = false;
    for (size_t i = 0; i < t.size();) {
        if (!placed && t.compare(i, kValue.size(), kValue) == 0) {
            out += conversion ? conversion : "";
            placed = true;
            i += kValue.size();
            continue;
        }
        if (t[i] == '%') out += '%';
        out += t[i++];
    }
    return out;
}

}  // namespace PickMyLoot
