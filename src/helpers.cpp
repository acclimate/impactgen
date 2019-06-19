#include "helpers.h"

namespace impactgen {

std::string fill_template(const std::string& in, const TemplateFunction& f) {
    const char* beg_mark = "[[";
    const char* end_mark = "]]";
    std::ostringstream ss;
    std::size_t pos = 0;
    while (true) {
        std::size_t start = in.find(beg_mark, pos);
        std::size_t stop = in.find(end_mark, start);
        if (stop == std::string::npos) {
            break;
        }
        ss.write(&*in.begin() + pos, start - pos);
        start += std::strlen(beg_mark);
        std::string key = in.substr(start, stop - start);
        ss << f(key, in);
        pos = stop + std::strlen(end_mark);
    }
    ss << in.substr(pos, std::string::npos);
    return ss.str();
}

std::string replace_all(const std::string& in, const std::string& to_replace, std::string replace_with) {
    auto res = in;
    auto pos = res.find(to_replace);
    while (pos != std::string::npos) {
        res.replace(pos, to_replace.size(), replace_with);
        pos = res.find(to_replace, pos + replace_with.size());
    }
    return res;
}

}  // namespace impactgen
