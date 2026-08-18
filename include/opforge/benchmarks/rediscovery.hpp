#pragma once
#include "opforge/patterns/analyzer.hpp"
#include "opforge/numerics/executor.hpp"
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
}
