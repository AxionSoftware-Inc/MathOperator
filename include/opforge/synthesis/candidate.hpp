#pragma once
#include "opforge/patterns/analyzer.hpp"
#include "opforge/synthesis/registry.hpp"
#include <map>
#include <string>
#include <vector>

namespace opforge::synthesis {
struct OperatorRequirements {
  atlas::SpaceRef domain, codomain;
  int max_order{-1};
  bool linear{true}, local{true};
  std::vector<std::string> required_structures, assumptions, must_annihilate;
};
struct Score {
  double structural_fit{0}, type_completeness{0}, identity_compatibility{0}, simplicity{0}, non_triviality{0}, generalization_power{0}, recoverability{0}, verification{0};
  double total() const { return structural_fit+type_completeness+identity_compatibility+simplicity+non_triviality+generalization_power+recoverability+verification; }
};
struct InterestingnessScore {
  double semantic_novelty{0}, generalization_power{0}, recovery_power{0}, independent_relations{0};
  double compression_reduction{0}, invariant_potential{0}, cross_domain_reach{0}, computational_utility{0};
  double non_triviality{0}, evidence_strength{0};
  std::vector<std::string> reasons;
  double total() const { return semantic_novelty + generalization_power + recovery_power + independent_relations + compression_reduction + invariant_potential + cross_domain_reach + computational_utility + non_triviality + evidence_strength; }
};
struct CandidateLineage { std::vector<std::string> atlas_operators, source_patterns, source_gaps, abstractions, requirements, construction_rules; };
struct OperatorCandidate {
  std::string id, construction_rule, novelty_status, rejection_reason, canonical_form, category;
  atlas::OperatorSignature signature;
  atlas::ExpressionPtr expression;
  std::vector<std::string> assumptions, required_structures, derived_properties, expected_identities;
  atlas::VerificationStatus verification{atlas::VerificationStatus::Proposed};
  Score score;
  InterestingnessScore interestingness;
  std::string semantic_category;
  struct SemanticEquivalence { int level{0}; bool equivalent{false}; std::string matched_id, explanation; std::vector<std::string> assumptions; } equivalence;
  CandidateLineage lineage;
};
struct CandidateReport { std::vector<OperatorCandidate> accepted, rejected; };
enum class EquivalenceLevel { Syntactic, ExactAST=Syntactic, Canonical, CanonicalAlgebraic=Canonical, Symbolic, IdentityRewrite, Numerical, NumericalUnderAssumptions=Numerical, Structural, StructurallyRelated=Structural, OperatorFamily, DecompositionComponent, Unknown };

class CandidateSynthesizer {
public:
  CandidateReport synthesize(const atlas::Atlas& atlas, const patterns::PatternReport& patterns) const;
  CandidateReport synthesize(const atlas::Atlas& atlas, const OperatorRequirements& requirements, const std::string& source) const;
  bool promote(atlas::Atlas& atlas, const OperatorCandidate& candidate) const;
  std::string export_text(const CandidateReport& report) const;
  std::string export_json(const CandidateReport& report) const;
};
std::string canonical(const atlas::ExpressionPtr& expression);
bool is_trivial(const atlas::ExpressionPtr& expression, const atlas::Atlas& atlas, std::string* reason=nullptr);
EquivalenceLevel classify_equivalence(const atlas::ExpressionPtr& left, const atlas::ExpressionPtr& right, const atlas::Atlas& atlas);
OperatorCandidate::SemanticEquivalence compare_semantics(const atlas::ExpressionPtr&, const atlas::Atlas&);
InterestingnessScore calculate_interestingness(const OperatorCandidate&, const atlas::Atlas&, const patterns::PatternReport&);
const char* to_string(EquivalenceLevel);
}
