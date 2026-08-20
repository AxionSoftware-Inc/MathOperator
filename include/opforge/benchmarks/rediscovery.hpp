#pragma once
#include "opforge/patterns/analyzer.hpp"
#include "opforge/numerics/executor.hpp"
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace opforge::benchmarks {
enum class RediscoveryOutcome { Exact, StructuralEquivalent, Partial, Missed, FalsePositive };
struct RediscoveryCase { std::string id, description; std::vector<std::string> hidden_knowledge, allowed_primitives, forbidden_relations; int budget{100}; bool strict_blind{false}; };
struct RediscoveryScorecard { int exact{0}, structural{0}, partial{0}, missed{0}, false_positive{0}, leakage_events{0}, total_patterns{0}, accepted_matches{0}, duplicate_abstractions{0}, weak_families{0}; double precision{0}, recall{0}, false_discovery_rate{0}; int cycles{0}, experiments{0}; };
struct RediscoveryResult { std::string benchmark_id; RediscoveryOutcome outcome{RediscoveryOutcome::Missed}; std::vector<std::string> discoveries; std::string explanation; };
struct RediscoveryReport { RediscoveryScorecard score; std::vector<RediscoveryResult> results; };
class HistoricalRediscovery {
public: RediscoveryReport run(const atlas::Atlas&, const std::vector<RediscoveryCase>&) const;
  std::string export_text(const RediscoveryReport&) const;
  std::string export_json(const RediscoveryReport&) const;
};
const char* to_string(RediscoveryOutcome);

// HistoricalRediscovery is intentionally retained for compatibility with the
// pre-Phase-0 reports. It is a legacy/demo harness: its case IDs select
// target-specific scoring and one case uses numerical probing. The types below
// define the current target-blind regression boundary.
enum class BlindOutcome {
  Exact, StructuralEquivalent, Partial, Missed, FalsePositive,
  NegativeControlPass, InvalidBenchmark
};

struct BlindTarget {
  enum class Kind { Composition, ZeroComposition, Relation, NoForbiddenComposition };
  Kind kind{Kind::Composition};
  std::string first_operator;
  std::string second_operator;
  atlas::RelationKind relation_kind{atlas::RelationKind::RelatedTo};
};

struct BlindRediscoveryCase {
  std::string id, description;
  std::set<std::string> hidden_identity_ids, hidden_relation_keys, hidden_operator_ids;
  BlindTarget target;
  bool negative_control{false};
  patterns::PatternBudget pattern_budget{};
};

struct BlindRediscoveryResult {
  std::string benchmark_id, description, explanation;
  std::string candidate_id_digest;
  BlindOutcome outcome{BlindOutcome::Missed};
  size_t graph_candidates{0}, compatible_edges{0}, pattern_candidates{0}, retained_patterns{0};
  size_t generated_candidates{0}, canonical_classes{0}, canonical_duplicates{0}, rejected_candidates{0};
  bool truncated{false};
  int numerical_experiments{0};
  std::vector<std::string> discoveries;
};

struct BlindRediscoveryScorecard {
  int exact{0}, structural{0}, partial{0}, missed{0}, false_positive{0};
  int negative_control_pass{0}, invalid_benchmarks{0}, leakage_events{0};
  size_t graph_candidates{0}, compatible_edges{0}, patterns{0}, generated_candidates{0};
  size_t canonical_classes{0}, canonical_duplicates{0}, rejected_candidates{0};
  int numerical_experiments{0};
  double exact_rate{0}, structural_rate{0}, structural_recovery_rate{0}, partial_rate{0},
      miss_rate{0}, false_positive_rate{0}, negative_control_pass_rate{0}, useful_signal_rate{0};
  // Deprecated compatibility field. It equals structural_recovery_rate and is
  // not exported as recall because partial recovery is not full recovery.
  double recall{0}, precision{0}, false_discovery_rate{0};
};

struct BlindRediscoveryReport {
  BlindRediscoveryScorecard score;
  std::vector<BlindRediscoveryResult> results;
  bool engine_knows_targets{false};
};

class BlindRediscoveryHarness {
public:
  BlindRediscoveryReport run(const atlas::Atlas&, const std::vector<BlindRediscoveryCase>&) const;
  std::string export_text(const BlindRediscoveryReport&) const;
  std::string export_json(const BlindRediscoveryReport&) const;
};

struct ScalingRun {
  std::string id, label;
  size_t operators{0}, spaces{0}, compatible_edges{0}, raw_candidate_count{0};
  size_t canonical_classes{0}, canonical_duplicates{0}, rejected_candidates{0};
  size_t pruned_candidates{0}, maximum_retained_frontier{0};
  int numerical_experiments{0};
  double runtime_ms{0};
  bool truncated{false};
};

struct ScalingReport { std::vector<ScalingRun> runs; };

class ScalingRegression {
public:
  ScalingReport run(const atlas::Atlas&, size_t medium_operator_count = 50) const;
  std::string export_text(const ScalingReport&) const;
  std::string export_json(const ScalingReport&) const;
};

std::vector<BlindRediscoveryCase> default_blind_rediscovery_cases();
const char* to_string(BlindOutcome);
}
