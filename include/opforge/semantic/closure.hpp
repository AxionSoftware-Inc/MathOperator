#pragma once

#include "opforge/atlas/model.hpp"

#include <map>
#include <string>
#include <vector>

namespace opforge::semantic {

enum class DerivationRuleKind { EqualitySubstitution, CompositionSubstitution, InverseCancellation,
                                AdjointReversal, ProjectionIdempotence, ChainComplex,
                                DecompositionSubstitution, TransformConjugation, SpectralConsequence };

struct DerivationRule {
  std::string id, name, premise_schema, conclusion_schema, scope, provenance;
  DerivationRuleKind kind{DerivationRuleKind::EqualitySubstitution};
  std::vector<std::string> assumptions;
};

struct DerivedRelation {
  std::string source, target, rule_id, condition, provenance;
  atlas::RelationKind kind{atlas::RelationKind::GeneratedBy};
  int depth{1};
  bool accepted{false};
};

struct DerivedConsequence {
  std::string id, rule_id, canonical_form, provenance, rejection_reason;
  atlas::Identity identity;
  std::vector<std::string> premises, assumptions;
  int depth{1};
  bool accepted{false}, duplicate{false}, contradiction{false}, type_valid{false};
};

struct SemanticConflict {
  std::string id, left_fact, right_fact, reason;
  std::vector<std::string> assumptions, provenance;
};

struct ClosureConfig {
  int max_depth{2};
  int max_consequences{160};
  double minimum_information_gain{0.05};
  bool include_derived_relations{true};
};

struct ClosureReport {
  atlas::Atlas closed_atlas;
  std::vector<DerivationRule> rules;
  std::vector<DerivedConsequence> consequences;
  std::vector<DerivedRelation> relations;
  std::vector<SemanticConflict> conflicts;
  int generated{0}, accepted{0}, duplicates{0}, pruned{0}, contradictions{0}, max_depth_reached{0};
  bool dag_acyclic{true};
};

struct DomainDensity {
  size_t operators{0}, relations{0}, identities{0}, derived_consequences{0}, bridges{0};
  double average_semantic_degree{0.0};
  std::vector<std::string> isolated_operators;
};

struct DensityReport {
  std::map<std::string, DomainDensity> domains;
  size_t operators{0}, relations{0}, identities{0}, derived_consequences{0}, bridges{0};
  double average_degree{0.0}, median_degree{0.0}, cross_domain_edge_ratio{0.0};
  size_t connected_components{0};
  std::vector<std::string> high_degree_hubs, isolated_operators;
};

struct PredictionOpportunity {
  std::string id, category, target_identity, difficulty, source_domain, target_domain;
  std::vector<std::string> visible_premises, assumptions;
  bool nontrivial{false}, leakage_free{true};
};

struct WithholdingCase {
  std::string id, category, difficulty, hidden_target, miss_reason;
  int visible_facts{0}, premise_count{0};
  bool attempted{false}, success{false}, leakage_free{true}, out_of_domain{false};
  std::vector<std::string> visible_premises, derivation_rules, assumptions;
};

struct RealBenchmarkReport {
  std::vector<PredictionOpportunity> opportunities;
  std::vector<WithholdingCase> cases;
  int attempts{0}, successes{0}, leakage_failures{0}, explicit_known_before_hide{0};
  int out_of_domain_attempts{0}, out_of_domain_successes{0};
};

struct ABCSnapshot {
  std::string name;
  size_t operators{0}, relations{0}, identities{0}, derived_consequences{0};
  int benchmark_attempts{0}, benchmark_successes{0}, out_of_domain_attempts{0}, out_of_domain_successes{0};
  int unknown_hypotheses{0}, under_specified_leads{0};
};

struct ABCAbulationReport {
  ABCSnapshot a, b, c;
  std::string conclusion;
};

class ConsequenceClosureEngine {
public:
  std::vector<DerivationRule> rule_catalog() const;
  ClosureReport close(const atlas::Atlas&, const ClosureConfig& = {}) const;
  DensityReport density(const atlas::Atlas&, const ClosureReport* = nullptr) const;
  std::vector<PredictionOpportunity> opportunities(const ClosureReport&) const;
  RealBenchmarkReport run_real_benchmark_v2(const atlas::Atlas&, const ClosureConfig& = {}) const;
  ABCAbulationReport run_abc_ablation(const atlas::Atlas&, const atlas::Atlas&, const ClosureConfig& = {}) const;
  std::string export_text(const ClosureReport&) const;
  std::string export_json(const ClosureReport&) const;
};

const char* to_string(DerivationRuleKind);

}  // namespace opforge::semantic
