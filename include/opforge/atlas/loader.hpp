#pragma once
#include "opforge/atlas/model.hpp"
#include <string>
#include <map>
#include <set>
namespace opforge::atlas {
struct AtlasStats {
  size_t operators{0}, spaces{0}, relations{0}, identities{0};
  size_t executable_equalities{0}, semantic_statements{0};
  // verified_facts means numerical/symbolic/formal evidence. Curated/imported
  // records are reported separately instead of being counted as proofs.
  size_t verified_facts{0}, partially_verified_facts{0}, unverified_facts{0};
  size_t disconnected{0}, unsupported_numerical{0};
  std::map<std::string,size_t> operators_by_domain, provenance_breakdown,
      relation_provenance_breakdown, identity_provenance_breakdown;
};
struct AtlasAuditReport {
  size_t contradictory_identities{0}, impossible_dimension_assumptions{0}, relation_cycles{0}, missing_identity_assumptions{0}, duplicate_relations{0}, bridge_type_mismatches{0};
  size_t invalid_space_operator_compatibility{0}, impossible_variance{0}, degree_mismatches{0}, inconsistent_adjoint_pairs{0}, inconsistent_bridge_direction{0}, accidental_analogue_equivalence{0}, unsupported_infinite_dimensional_claims{0}, duplicate_semantic_facts{0}, circular_generalization{0};
  std::vector<std::string> missing_identity_assumption_ids;
  std::vector<AtlasIssue> issues;
  bool clean() const { return issues.empty(); }
};
class AtlasLoader {
public: static Atlas load(const std::string& path); static AtlasStats stats(const Atlas&); static std::vector<AtlasIssue> validate(const Atlas&);
  static Atlas load_excluding(const std::string& path, const std::set<std::string>& excluded_files);
  static AtlasAuditReport audit_v2(const Atlas&); static AtlasAuditReport audit_v3(const Atlas&);
  struct DiversityReport { std::vector<std::string> domains, structure_kinds, relation_kinds, assumption_regimes, isolated_operators; std::map<std::string,size_t> operators_per_domain, relations_per_domain, identities_per_domain, bridges_per_domain_pair; size_t continuous_operators{0}, discrete_operators{0}, continuous_discrete_bridges{0}, independent_realizations{0}; bool algebraic_coverage{false}, geometric_coverage{false}, spectral_coverage{false}, variational_coverage{false}, discrete_coverage{false}; };
  static DiversityReport diversity(const Atlas&);
};
}
