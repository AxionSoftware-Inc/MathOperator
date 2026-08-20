#pragma once

#include "opforge/semantic/core.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace opforge::search {

using semantic::Constraint;
using semantic::Context;
using semantic::EpistemicStatus;
using semantic::ExpressionPtr;
using semantic::Judgment;
using semantic::SemanticId;
using semantic::Theory;
using semantic::TypeCheckStatus;
using semantic::TypeRef;
using semantic::ValidityRegime;

enum class ReductionKind { Lossless, Lossy, Unresolved, Retained };

enum class ReductionReason {
  RetainedRepresentative,
  TypeInvalid,
  RegimeIncompatible,
  ExactDuplicate,
  CanonicalDuplicate,
  ProvenEquivalent,
  SymmetryEquivalent,
  KnownConsequence,
  Degenerate,
  DominatedLossless,
  DepthLimit,
  FrontierBudget,
  ResourceLimit,
  Unsupported,
  Unknown
};

enum class TerminationStatus {
  ExhaustedRelativeSpace,
  BudgetEnded,
  ResourceLimit,
  InvalidScope,
  IncompleteUnknown,
  Failed
};

const char* to_string(ReductionKind);
const char* to_string(ReductionReason);
const char* to_string(TerminationStatus);

struct SearchScope {
  SemanticId id;
  SemanticId theory_id;
  std::string theory_version;
  std::string grammar_id;
  std::vector<std::string> allowed_construction_kinds;
  std::size_t max_depth{0};
  std::size_t candidate_budget{0};
  std::size_t frontier_budget{0};
  std::size_t resource_limit_ms{0};
  SemanticId equivalence_theory_id;
  SemanticId context_id;
  ValidityRegime regime;
  std::uint64_t deterministic_seed{0};

  void refresh_id();
  std::string canonical() const;
  bool valid(std::string* reason = nullptr) const;
};

struct Construction {
  SemanticId id;
  std::string grammar_rule;
  std::size_t depth{0};
  std::uint64_t ordinal{0};
  ExpressionPtr expression;
  std::vector<Constraint> side_conditions;
  std::optional<Judgment> proposition;

  void refresh_id();
  std::string canonical() const;
};

struct SymmetryRule {
  SemanticId id;
  std::string name;
  std::string domain;
  SemanticId context_id;
  ValidityRegime regime;
  ExpressionPtr source;
  ExpressionPtr target;
  Judgment certificate;

  void refresh_id();
  std::string canonical() const;
};

struct EquivalenceCertificate {
  ReductionReason reason{ReductionReason::CanonicalDuplicate};
  SemanticId rule_id;
  SemanticId source_construction_id;
  SemanticId context_id;
  SemanticId regime_id;
  std::string detail;

  std::string canonical() const;
};

struct EquivalenceMember {
  SemanticId construction_id;
  std::string structural_key;
  EquivalenceCertificate certificate;

  std::string canonical() const;
};

struct EquivalenceClass {
  SemanticId id;
  std::string structural_key;
  Construction representative;
  TypeCheckStatus type_status{TypeCheckStatus::Unknown};
  TypeRef type{TypeRef::unknown()};
  std::size_t member_count{0};
  std::string member_digest;
  std::vector<EquivalenceMember> members;
  std::vector<EquivalenceCertificate> certificates;

  std::string canonical() const;
};

struct LedgerRecord {
  SemanticId construction_id;
  ReductionKind kind{ReductionKind::Retained};
  ReductionReason reason{ReductionReason::RetainedRepresentative};
  SemanticId class_id;
  SemanticId representative_id;
  std::string detail;

  std::string canonical() const;
};

struct PruningLedger {
  std::map<ReductionReason, std::size_t> counts;
  std::vector<LedgerRecord> records;
  std::string record_digest;

  void record(const LedgerRecord& entry, bool retain_record);
  std::size_t count(ReductionReason reason) const;
  std::string canonical() const;
};

struct QuotientMetrics {
  std::size_t raw_constructions{0};
  std::size_t type_valid{0};
  std::size_t type_invalid{0};
  std::size_t type_unknown{0};
  std::size_t regime_compatible{0};
  std::size_t regime_incompatible{0};
  std::size_t regime_unknown{0};
  std::size_t lossless_reductions{0};
  std::size_t lossy_reductions{0};
  std::size_t unresolved_candidates{0};
  std::size_t retained_classes{0};
  std::size_t peak_retained_frontier{0};
  double runtime_ms{0.0};
};

struct QuotientSearchOptions {
  bool retain_ledger_records{true};
  bool retain_member_records{true};
  std::size_t sample_limit{32};
  std::function<void(const LedgerRecord&)> ledger_sink;
};

struct QuotientSearchResult {
  SearchScope scope;
  TerminationStatus termination{TerminationStatus::Failed};
  std::string termination_reason;
  QuotientMetrics metrics;
  PruningLedger ledger;
  std::vector<EquivalenceClass> classes;

  bool relative_complete() const { return termination == TerminationStatus::ExhaustedRelativeSpace; }
  std::string canonical() const;
};

using ConstructionSource = std::function<std::optional<Construction>()>;

std::string canonical_structural_key(const ExpressionPtr& expression,
                                     const Context& context,
                                     const ValidityRegime& regime,
                                     const TypeRef& type);

class QuotientSearchEngine {
public:
  QuotientSearchResult run(const Theory&, const Context&, const SearchScope&,
                           ConstructionSource,
                           const std::vector<SymmetryRule>& symmetry_rules = {},
                           QuotientSearchOptions options = {}) const;

  QuotientSearchResult run(const Theory&, const Context&, const SearchScope&,
                           const std::vector<Construction>& constructions,
                           const std::vector<SymmetryRule>& symmetry_rules = {},
                           QuotientSearchOptions options = {}) const;
};

struct FiniteBenchmarkReport {
  QuotientSearchResult exhaustive;
  QuotientSearchResult budgeted;
  std::size_t reference_raw_constructions{0};
  std::size_t reference_classes{0};
};

struct SyntheticBenchmarkReport {
  std::size_t requested_raw_constructions{0};
  QuotientSearchResult result;
};

struct ScalingBenchmarkRun {
  std::size_t operators{0};
  QuotientSearchResult result;
};

struct Layer16BenchmarkReport {
  FiniteBenchmarkReport finite;
  SyntheticBenchmarkReport synthetic;
  std::vector<ScalingBenchmarkRun> scaling;
};

FiniteBenchmarkReport run_finite_reference_benchmark();
SyntheticBenchmarkReport run_synthetic_stream_benchmark(std::size_t raw_constructions = 1000000);
std::vector<ScalingBenchmarkRun> run_atlas_scaling_benchmark(const atlas::Atlas&, const std::vector<std::size_t>& sizes = {12, 50, 98});
Layer16BenchmarkReport run_layer16_benchmarks(const atlas::Atlas&);

std::string export_text(const QuotientSearchResult&);
std::string export_json(const QuotientSearchResult&);
std::string export_text(const Layer16BenchmarkReport&);
std::string export_json(const Layer16BenchmarkReport&);

}  // namespace opforge::search
