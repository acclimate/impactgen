#include "impacts/WbgtLabourProductivity.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

#include "Output.h"
#include "ReferenceTime.h"
#include "helpers.h"
#include "netcdftools.h"

namespace impactgen {
namespace {

// Kjellstrom, Holmer and Lemke (2009), Table 2, DOI 10.3402/gha.v2i0.2047.
// Linear interpolation of remaining capacity for acclimatised workers in light
// clothing.
double labour_capacity(double wbgt, int work_intensity) {
    static constexpr std::array<std::array<double, 5>, 4> thresholds = {
        {{{31, 31.5, 32, 32.5, 39}}, {{28, 29, 30.5, 32, 37}}, {{27, 27.5, 29.5, 31.5, 36}}, {{25.5, 26.5, 28, 31, 34}}}};
    const auto& knots = thresholds[(work_intensity - 200) / 100];
    if (wbgt <= knots.front()) return 1.0;
    for (std::size_t i = 1; i < knots.size(); ++i) {
        if (wbgt <= knots[i]) {
            return 1.0 - 0.25 * (static_cast<double>(i - 1) + (wbgt - knots[i - 1]) / (knots[i] - knots[i - 1]));
        }
    }
    return 0.0;
}

struct WbgtField {
    netCDF::NcFile file;
    netCDF::NcVar variable;
    GeoGrid<float> grid;
    std::vector<std::time_t> times;
    std::string filename;
    double scale = 1.0;
    double offset = 0.0;
    double unit_offset = 0.0;
    double decoded_missing = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> missing;

    WbgtField(const settings::SettingsNode& node, const TemplateFunction& template_func, int time_shift)
        : filename(fill_template(node["file"].as<std::string>(), template_func)) {
        file.open(filename, netCDF::NcFile::read);
        variable = file.getVar(node["variable"].as<std::string>());
        if (variable.isNull() ||
            (!check_dimensions(variable, {"time", "lat", "lon"}) && !check_dimensions(variable, {"time", "latitude", "longitude"}))) {
            throw std::runtime_error(filename + ": Expected WBGT variable with dimensions time, lat, lon");
        }
        grid.read_from_netcdf(file, filename);
        const auto attributes = variable.getAtts();
        auto units = node["units"].as<std::string>("");
        if (units.empty()) {
            const auto att = attributes.find("units");
            if (att != attributes.end()) {
                att->second.getValues(units);
            }
        }
        if ((units == "K" || units == "kelvin")) {
            unit_offset = -273.15;
        } else if ((units == "degC" || units == "degrees_Celsius" || units == "degree_Celsius")) {
            unit_offset = 0;
        } else {
            throw std::runtime_error(filename + ": Unsupported WBGT units '" + units + "'");
        }
        if (node.has("missing_value")) {
            decoded_missing = node["missing_value"].as<double>();
            if (!std::isfinite(decoded_missing)) throw std::runtime_error("WBGT missing_value must be finite");
        }
        for (const auto& name : {"_FillValue", "missing_value"}) {
            const auto att = attributes.find(name);
            if (att != attributes.end()) {
                std::vector<double> values(att->second.getAttLength());
                att->second.getValues(values.data());
                missing.insert(missing.end(), values.begin(), values.end());
            }
        }
        const auto scale_att = attributes.find("scale_factor");
        const auto offset_att = attributes.find("add_offset");
        if (scale_att != attributes.end()) {
            if (scale_att->second.getAttLength() != 1) {
                throw std::runtime_error(filename + ": scale_factor must be scalar");
            }
            scale_att->second.getValues(&scale);
        }
        if (offset_att != attributes.end()) {
            if (offset_att->second.getAttLength() != 1) {
                throw std::runtime_error(filename + ": add_offset must be scalar");
            }
            offset_att->second.getValues(&offset);
        }
        if (!std::isfinite(scale) || !std::isfinite(offset)) {
            throw std::runtime_error(filename + ": Invalid packing attributes");
        }

        const auto time = file.getVar("time");
        if (time.isNull() || !check_dimensions(time, {"time"})) {
            throw std::runtime_error(filename + ": Missing time coordinate");
        }
        const auto time_attributes = time.getAtts();
        const auto calendar_att = time_attributes.find("calendar");
        std::string calendar = "standard";
        if (calendar_att != time_attributes.end()) {
            calendar_att->second.getValues(calendar);
        }
        if (calendar != "standard" && calendar != "gregorian" && calendar != "proleptic_gregorian") {
            throw std::runtime_error(filename + ": Unsupported calendar '" + calendar + "'; convert to Gregorian before running");
        }
        std::string time_units;
        time.getAtt("units").getValues(time_units);
        const ReferenceTime reference(time_units);
        const auto origin = reference.unreference(0);
        const auto seconds_per_unit = reference.unreference(1) - origin;
        std::vector<double> values(time.getDim(0).getSize());
        if (values.empty()) {
            throw std::runtime_error(filename + ": Empty time coordinate");
        }
        time.getVar(values.data());
        for (const auto value : values) {
            const auto seconds = (value + time_shift) * static_cast<double>(seconds_per_unit);
            if (!std::isfinite(seconds) || std::abs(seconds) > 1e12) {
                throw std::runtime_error(filename + ": Invalid time coordinate");
            }
            times.push_back(origin + static_cast<std::time_t>(std::llround(seconds)));
        }
    }

    std::vector<double> read(std::size_t start, std::size_t count) const {
        std::vector<double> values(count * grid.size());
        variable.getVar({start, 0, 0}, {count, grid.lat_count, grid.lon_count}, values.data());
        for (auto& value : values) {
            if (!std::isfinite(value) || std::abs(value) > 1e20 || std::find(missing.begin(), missing.end(), value) != missing.end()) {
                value = std::numeric_limits<double>::quiet_NaN();
            } else {
                value = value * scale + offset;
                value = value == decoded_missing ? std::numeric_limits<double>::quiet_NaN() : value + unit_offset;
            }
        }
        return values;
    }
};

// Match cell centres without wrapping, interpolating or dropping the last
// row/column.
long cell_index(double coordinate, double first, double step, std::size_t count) {
    const auto position = (coordinate - first) / step;
    const auto index = std::lround(position);
    if (index < 0 || index >= static_cast<long>(count)) {
        return -1;
    }
    if (std::abs(position - static_cast<double>(index)) > 1e-3) {
        throw std::runtime_error("WBGT, proxy and ISO raster cell centres must align");
    }
    return index;
}

std::time_t day_start(std::time_t time) { return static_cast<std::time_t>(std::floor(static_cast<double>(time) / 86400.0)) * 86400; }

}  // namespace

WbgtLabourProductivity::WbgtLabourProductivity(const settings::SettingsNode& impact_node, AgentForcing base_forcing_p)
    : AgentImpact(std::move(base_forcing_p)), ProxiedImpact(impact_node["proxy"]), Impact(impact_node), climate(impact_node["climate"]) {
    if (!climate.has("wbgt") || climate.has("temperature") || climate.has("relative_humidity") || climate.has("maximum_temperature") ||
        climate.has("maximum_wbgt")) {
        throw std::runtime_error(
            "wbgt_labour_productivity requires only "
            "precomputed hourly climate.wbgt");
    }
    if (impact_node["time_resolution"].as<std::string>("hourly") != "hourly" || impact_node.has("working_hours")) {
        throw std::runtime_error(
            "WBGT productivity requires hourly input and "
            "complete 24-hour UTC days");
    }
    if (chunk_size == 0) {
        throw std::runtime_error("chunk_size must be positive");
    }
    std::set<int> assigned;
    for (const auto node : impact_node["worker_categories"].as_sequence()) {
        WorkerCategory category;
        category.work_intensity = node["work_intensity"].as<int>();
        if (category.work_intensity < 200 || category.work_intensity > 500 || category.work_intensity % 100 != 0) {
            throw std::runtime_error("work_intensity must be 200, 300, 400 or 500 W");
        }
        if (node.has("indoor"))
            throw std::runtime_error(
                "Supplied WBGT is used directly; indoor "
                "adjustments are not supported");
        for (const auto sector : node["sectors"].as_sequence()) {
            const auto index = static_cast<int>(base_forcing.get_sectors().at(sector.as<std::string>()));
            if (!assigned.insert(index).second) {
                throw std::runtime_error("Each sector must belong to only one worker category");
            }
            category.sectors.push_back(index);
        }
        if (category.sectors.empty()) {
            throw std::runtime_error("Worker categories require sectors");
        }
        categories.push_back(std::move(category));
    }
    if (categories.empty()) {
        throw std::runtime_error("worker_categories must not be empty");
    }
    read_isoraster(impact_node["isoraster"], base_forcing.get_regions());
}

void WbgtLabourProductivity::join(Output& output, const TemplateFunction& template_func) try {
    WbgtField wbgt(climate["wbgt"], template_func, time_shift);
    for (std::size_t t = 1; t < wbgt.times.size(); ++t) {
        if (wbgt.times[t] - wbgt.times[t - 1] != 3600) {
            throw std::runtime_error("WBGT timestamps must be consecutive hourly samples");
        }
    }
    if (wbgt.times.size() % 24 != 0 || wbgt.times.front() != day_start(wbgt.times.front())) {
        throw std::runtime_error("Hourly WBGT must contain complete UTC days starting at midnight");
    }
    read_proxy(fill_template(proxy_filename, template_func), output.get_regions());
    const auto& grid = wbgt.grid;
    if (!grid.is_compatible(isoraster_grid) || !grid.is_compatible(proxy_grid)) {
        throw std::runtime_error("WBGT, proxy and ISO raster resolutions must match");
    }
    struct Cell {
        std::size_t index;
        int region;
        double weight;
    };
    std::vector<Cell> cells;
    std::vector<double> weights(base_forcing.get_regions().size(), 0.0);
    for (std::size_t lat = 0; lat < grid.lat_count; ++lat) {
        const auto latitude = (grid.lat_stepsize > 0 ? grid.lat_min : grid.lat_max) + static_cast<double>(lat) * grid.lat_stepsize;
        const auto iso_lat = cell_index(latitude, isoraster_grid.lat_stepsize > 0 ? isoraster_grid.lat_min : isoraster_grid.lat_max,
                                        isoraster_grid.lat_stepsize, isoraster_grid.lat_count);
        const auto proxy_lat = cell_index(latitude, proxy_grid.lat_stepsize > 0 ? proxy_grid.lat_min : proxy_grid.lat_max, proxy_grid.lat_stepsize,
                                          proxy_grid.lat_count);
        if (iso_lat < 0 || proxy_lat < 0) {
            continue;
        }
        for (std::size_t lon = 0; lon < grid.lon_count; ++lon) {
            const auto longitude = (grid.lon_stepsize > 0 ? grid.lon_min : grid.lon_max) + static_cast<double>(lon) * grid.lon_stepsize;
            const auto iso_lon = cell_index(longitude, isoraster_grid.lon_stepsize > 0 ? isoraster_grid.lon_min : isoraster_grid.lon_max,
                                            isoraster_grid.lon_stepsize, isoraster_grid.lon_count);
            const auto proxy_lon = cell_index(longitude, proxy_grid.lon_stepsize > 0 ? proxy_grid.lon_min : proxy_grid.lon_max,
                                              proxy_grid.lon_stepsize, proxy_grid.lon_count);
            if (iso_lon < 0 || proxy_lon < 0) {
                continue;
            }
            const auto iso = isoraster(static_cast<std::size_t>(iso_lat), static_cast<std::size_t>(iso_lon));
            if (iso < 0 || static_cast<std::size_t>(iso) >= regions.size() || regions[iso] < 0) {
                continue;
            }
            const auto weight = proxy_values(static_cast<std::size_t>(proxy_lat), static_cast<std::size_t>(proxy_lon));
            if (!std::isfinite(weight) || weight <= 0) {
                continue;
            }
            cells.push_back({lat * grid.lon_count + lon, regions[iso], weight});
            weights[regions[iso]] += weight;
        }
    }
    if (cells.empty()) {
        throw std::runtime_error("No positive proxy coverage in the climate/ISO raster overlap");
    }

    auto series = ForcingSeries<AgentForcing>(base_forcing, output.ref());
    for (std::size_t start = 0; start < wbgt.times.size(); start += 24) {
        std::vector<std::vector<double>> capacity(categories.size(), std::vector<double>(weights.size(), 0.0));
        for (std::size_t pos = start; pos < start + 24;) {
            const auto count = std::min(chunk_size, start + 24 - pos);
            const auto values = wbgt.read(pos, count);
            for (std::size_t t = 0; t < count; ++t) {
                for (const auto& cell : cells) {
                    const auto value = values[t * grid.size() + cell.index];
                    if (!std::isfinite(value) || value <= -100 || value > 100) {
                        throw std::runtime_error(
                            "Invalid or missing WBGT at a "
                            "positive-proxy cell (time index " +
                            std::to_string(pos + t) + ", cell " + std::to_string(cell.index) + ")");
                    }
                    for (std::size_t k = 0; k < categories.size(); ++k) {
                        capacity[k][cell.region] += labour_capacity(value, categories[k].work_intensity) * cell.weight;
                    }
                }
            }
            pos += count;
        }
        auto& forcing = series.insert_forcing(wbgt.times[start]);
        for (std::size_t k = 0; k < categories.size(); ++k) {
            for (std::size_t region = 0; region < weights.size(); ++region) {
                if (weights[region] <= 0) continue;
                for (const auto sector : categories[k].sectors) {
                    forcing(sector, region) = static_cast<ForcingType>(std::max(0.0, std::min(1.0, capacity[k][region] / (weights[region] * 24))));
                }
            }
        }
    }
    output.include_forcing(series);
} catch (const netCDF::exceptions::NcException& e) {
    throw std::runtime_error("WBGT labour productivity: " + std::string(e.what()));
}

}  // namespace impactgen
