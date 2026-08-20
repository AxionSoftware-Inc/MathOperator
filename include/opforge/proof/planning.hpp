#pragma once

#include "opforge/reasoning/bidirectional.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace opforge::proof {

using semantic::Context;
using semantic::Evidence;
using semantic::EpistemicStatus;
using semantic::ExpressionPtr;
using semantic::Judgment;
using semantic::ProofObligation;
using semantic::ProofObligationStatus;
using semantic::Provenance;
using semantic::SemanticId;
using semantic::Theory;
using semantic::ValidityRegime;

// These are capability labels, not a total ordering.  In particular,
// NumericalSupportOnly is never an alternate spelling for Symbolic or Formal.
enum class EvidenceLevel {
  Structural,
  TrustedFact,
  Symbolic,
  Formal,
  NumericalSupportOnly
};

enum class ProofPlanStatus {
  CompleteAtRequiredLevel,
  IncompleteOpenObligations,
  BlockedUnknown,
  Falsified,
  Contradicted,
  Cyclic,
  Unsupported,
  InvalidDerivation
};

enum class ProofNodeKind { Obligation, RuleApplication, Evidence };
enum class ProofEdgeKind { DependsOn, DerivedBy, Alternative, Supports };
enum class ProofRuleKind { ExplicitRule, StructuralLineage, TypeCheck, RegimeCheck, SideCondition, TrustedFact };
enum class CertificateStatus { Unchecked, Accepted, Rejected, Invalidated };

const char* to_string(EvidenceLevel);
const char* to_string(ProofPlanStatus);
const char* to_string(ProofNodeKind);
const char* to_string(ProofEdgeKind);
const char* to_string(ProofRuleKind);
const char* to_string(CertificateStatus);

// A backend-neutral envelope for future Layer-19 certificates.  Layer 18
// stores and audits this value but does not verify external payloads.
struct ProofCertificate {
  SemanticId id;
  SemanticId obligation_id;
  std::string backend;
  EvidenceLevel evidence_level{EvidenceLevel::Structural};
  std::string deterministic_payload;
  CertificateStatus status{CertificateStatus::Unchecked};
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

// A proof rule is deliberately separate from a Layer-17 GoalRule.  A caller
// must explicitly provide proof-safe provenance; a search rule is never
// silently promoted to a proof rule.
struct ProofRule {
  SemanticId id;
  std::string name;
  ProofRuleKind kind{ProofRuleKind::ExplicitRule};
  Context pattern_context;
  Judgment conclusion;
  std::vector<Judgment> premises;
  std::vector<semantic::Constraint> conditions;
  ValidityRegime regime;
  Provenance provenance;
  EvidenceLevel evidence_level{EvidenceLevel::Structural};
  bool proof_safe{false};

  void refresh_id();
  std::string canonical() const;
};

// Explicit conversion is available for a reviewed integration boundary, but
// defaults to proof_safe=false.  Layer-17 search soundness is not proof
// soundness.
ProofRule proof_rule_from_goal_rule(const reasoning::GoalRule&, bool proof_safe = false,
                                    ProofRuleKind kind = ProofRuleKind::ExplicitRule);

struct ProofPlanningOptions {
  EvidenceLevel required_evidence{EvidenceLevel::Structural};
  bool generate_typing_obligations{true};
  bool generate_regime_obligations{true};
  bool retain_alternatives{true};
  std::size_t max_obligations{0};
};

struct ProofPlanNode {
  SemanticId id;
  ProofNodeKind kind{ProofNodeKind::Obligation};
  SemanticId obligation_id;
  SemanticId rule_id;
  SemanticId certificate_id;
  SemanticId context_id;
  SemanticId regime_id;
  ProofObligationStatus status{ProofObligationStatus::Unresolved};
  std::string label;
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct ProofPlanEdge {
  SemanticId id;
  SemanticId source_id;
  SemanticId target_id;
  ProofEdgeKind kind{ProofEdgeKind::DependsOn};
  SemanticId branch_id;
  SemanticId context_id;
  SemanticId regime_id;
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct ProofPlanAccounting {
  std::size_t generated_obligations{0};
  std::size_t unique_obligations{0};
  std::size_t duplicate_obligations{0};
  std::size_t automatically_discharged{0};
  std::size_t open{0};
  std::size_t unknown{0};
  std::size_t falsified{0};
  std::size_t contradicted{0};
  std::size_t cyclic{0};
  std::size_t unsupported{0};
  std::size_t numerically_supported{0};

  bool consistent() const;
  std::string canonical() const;
};

struct ProofPlan {
  SemanticId id;
  Judgment target;
  SemanticId structural_candidate_id;
  Context context;
  ValidityRegime regime;
  reasoning::GoalSearchScope scope;
  Provenance provenance;
  EvidenceLevel required_evidence{EvidenceLevel::Structural};
  ProofPlanStatus status{ProofPlanStatus::Unsupported};
  std::string status_reason;
  std::vector<SemanticId> root_obligation_ids;
  std::vector<ProofObligation> obligations;
  std::vector<ProofPlanNode> nodes;
  std::vector<ProofPlanEdge> edges;
  std::vector<ProofCertificate> certificates;
  std::vector<SemanticId> unresolved_obligation_ids;
  std::vector<std::vector<SemanticId>> cycles;
  ProofPlanAccounting accounting;

  void refresh_id();
  std::string identity_canonical() const;
  std::string canonical() const;
  bool accounting_consistent() const { return accounting.consistent(); }
};

class ProofPlanner {
public:
  // Primary Layer-17 integration path.  The selected candidate and its actual
  // goal/forward snapshots are consumed; expected answers are not accepted.
  ProofPlan plan(const reasoning::GoalSearchResult&, std::size_t solution_index,
                 const Theory&, const Context&, const std::vector<ProofRule>& = {},
                 const std::vector<ProofCertificate>& = {},
                 ProofPlanningOptions = {}) const;

  // Narrow direct path for unit fixtures and future callers that already have
  // a structural candidate but no retained GoalSearchResult snapshots.
  ProofPlan plan(const reasoning::SolutionCandidate&, const Theory&, const Context&,
                 const std::vector<ProofRule>& = {},
                 const std::vector<ProofCertificate>& = {},
                 ProofPlanningOptions = {}) const;

  // A target-only plan is useful for testing explicit proof-rule DAGs.  It does
  // not claim that a structural candidate exists.
  ProofPlan plan(const Judgment&, const Theory&, const Context&,
                 const std::vector<ProofRule>& = {},
                 const std::vector<ProofCertificate>& = {},
                 ProofPlanningOptions = {}) const;

  // Replay re-evaluates stored obligations against current semantic inputs.
  // Missing or changed evidence reopens dependents; no previous status is
  // trusted merely because it was present in the old plan.
  ProofPlan replay(const ProofPlan&, const Theory&, const Context&,
                   const std::vector<ProofRule>& = {},
                   const std::vector<ProofCertificate>& = {},
                   ProofPlanningOptions = {}) const;
};

struct Layer18BenchmarkOutcome {
  std::string id;
  std::string category;
  ProofPlan plan;
};

struct Layer18BenchmarkReport {
  std::vector<Layer18BenchmarkOutcome> outcomes;
  std::vector<Layer18BenchmarkOutcome> negative_controls;
  ProofPlan indexed_plan;
  std::string deterministic_digest;
};

Layer18BenchmarkReport run_layer18_benchmarks();
std::string export_text(const ProofPlan&);
std::string export_json(const ProofPlan&);
std::string export_text(const Layer18BenchmarkReport&);
std::string export_json(const Layer18BenchmarkReport&);

}  // namespace opforge::proof
