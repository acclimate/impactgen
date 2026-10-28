#ifndef IMPACTGEN_WBGTLABOURPRODUCTIVITY_H
#define IMPACTGEN_WBGTLABOURPRODUCTIVITY_H

#include "impacts/AgentImpact.h"
#include "impacts/Impact.h"
#include "impacts/ProxiedImpact.h"

namespace impactgen {

class WbgtLabourProductivity : public AgentImpact, public ProxiedImpact, public Impact {
    struct WorkerCategory {
        std::vector<int> sectors;
        int work_intensity;
    };
    settings::SettingsNode climate;
    std::vector<WorkerCategory> categories;

   public:
    WbgtLabourProductivity(const settings::SettingsNode& impact_node, AgentForcing base_forcing_p);
    void join(Output& output, const TemplateFunction& template_func) override;
};

}  // namespace impactgen
#endif
