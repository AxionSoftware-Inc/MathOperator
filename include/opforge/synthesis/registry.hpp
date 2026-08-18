#pragma once

#include "opforge/atlas/model.hpp"

#include <string>
#include <vector>

namespace opforge::synthesis {

struct KnownConstruction {
  std::string id, name, role, related_operator, decomposition_context, provenance;
  atlas::ExpressionPtr expression;
  std::vector<std::string> assumptions, sources;
  std::vector<atlas::VerificationEvidence> evidence;
};

class KnownConstructionRegistry {
public:
  static KnownConstructionRegistry from_atlas(const atlas::Atlas&);
  void add(KnownConstruction);
  const std::vector<KnownConstruction>& all() const { return constructions_; }
  const KnownConstruction* find(const std::string& id) const;
  const KnownConstruction* equivalent_to(const atlas::ExpressionPtr&, const atlas::Atlas&) const;
private:
  std::vector<KnownConstruction> constructions_;
};

}  // namespace opforge::synthesis
