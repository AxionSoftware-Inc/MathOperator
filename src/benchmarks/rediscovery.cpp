#include "opforge/benchmarks/rediscovery.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace opforge::benchmarks {

using atlas::Atlas;
using atlas::RelationKind;
using numerics::Grid;
using numerics::NumericObject;

const char* to_string(RediscoveryOutcome outcome) {
  switch (outcome) {
    case RediscoveryOutcome::Exact: return "exact";
    case RediscoveryOutcome::StructuralEquivalent: return "structural_equivalent";
    case RediscoveryOutcome::Partial: return "partial";
    case RediscoveryOutcome::Missed: return "missed";
    case RediscoveryOutcome::FalsePositive: return "false_positive";
  }
  return "unknown";
}

namespace {

std::vector<const atlas::OperatorRecord*> matching(const Atlas& atlas, const std::string& domain_prefix,
                                                   const std::string& codomain_prefix, int order,
                                                   bool continuous, bool discrete) {
  std::vector<const atlas::OperatorRecord*> result;
  for (const auto* op : atlas.all()) {
    if (op->signature.domain.id.rfind(domain_prefix, 0) == 0 &&
        op->signature.codomain.id.rfind(codomain_prefix, 0) == 0 &&
        op->signature.differential_order == order && op->signature.continuous == continuous &&
        op->signature.discrete == discrete) {
      result.push_back(op);
    }
  }
  return result;
}

bool has_relation(const Atlas& atlas, const std::string& source, RelationKind kind, const std::string& target) {
  const auto* op = atlas.find(source);
  if (!op) return false;
  return std::any_of(op->relations.begin(), op->relations.end(), [&](const auto& relation) {
    return relation.kind == kind && relation.target_id == target;
  });
}

bool has_pattern(const patterns::PatternReport& report, patterns::PatternType type) {
  return std::any_of(report.patterns.begin(), report.patterns.end(),
                     [&](const auto& pattern) { return pattern.type == type; });
}

void mark_found(RediscoveryResult& result, RediscoveryScorecard& score,
                RediscoveryOutcome outcome, std::string discovery, std::string explanation) {
  result.outcome = outcome;
  result.discoveries.push_back(std::move(discovery));
  result.explanation = std::move(explanation);
  switch (outcome) {
    case RediscoveryOutcome::Exact: ++score.exact; break;
    case RediscoveryOutcome::StructuralEquivalent: ++score.structural; break;
    case RediscoveryOutcome::Partial: ++score.partial; break;
    case RediscoveryOutcome::Missed: ++score.missed; break;
    case RediscoveryOutcome::FalsePositive: ++score.false_positive; break;
  }
  if (outcome != RediscoveryOutcome::Missed && outcome != RediscoveryOutcome::FalsePositive) ++score.accepted_matches;
}

}  // namespace

RediscoveryReport HistoricalRediscovery::run(const Atlas& source,
                                             const std::vector<RediscoveryCase>& cases) const {
  RediscoveryReport report;

  for (const auto& benchmark : cases) {
    const auto hidden_identity_ids = std::set<std::string>(benchmark.hidden_knowledge.begin(),
                                                            benchmark.hidden_knowledge.end());
    const auto hidden_relation_keys = std::set<std::string>(benchmark.forbidden_relations.begin(),
                                                             benchmark.forbidden_relations.end());
    const auto hidden = source.without_identities(hidden_identity_ids).without_relations(hidden_relation_keys);
    const auto analysis_atlas = benchmark.strict_blind ? hidden.neutralized() : hidden;
    const auto structural = patterns::PatternAnalyzer{}.analyze(analysis_atlas);

    RediscoveryResult result;
    result.benchmark_id = benchmark.id;
    result.explanation = "blind run over visible typed primitives";
    ++report.score.cycles;
    report.score.experiments += static_cast<int>(structural.graph.size());
    report.score.total_patterns += static_cast<int>(structural.patterns.size());
    report.score.weak_families += static_cast<int>(std::count_if(structural.patterns.begin(), structural.patterns.end(),
      [](const auto& pattern) { return pattern.type == patterns::PatternType::OperatorFamily && pattern.confidence < 0.8; }));

    // A benchmark is invalid if a declared hidden fact survives masking. This is
    // reported separately from a mathematical false positive so leakage cannot
    // inflate the research score.
    bool leakage = false;
    for (const auto& id : hidden_identity_ids) leakage |= hidden.find_identity(id) != nullptr;
    for (const auto& op : hidden.all()) {
      for (const auto& relation : op->relations) {
        const auto canonical = op->id + "|" + atlas::to_string(relation.kind) + "|" + relation.target_id;
        const auto arrow = op->id + "->" + relation.target_id;
        leakage |= hidden_relation_keys.contains(canonical) || hidden_relation_keys.contains(arrow);
      }
    }
    if (leakage) {
      mark_found(result, report.score, RediscoveryOutcome::FalsePositive,
                 "benchmark leakage detected", "declared hidden knowledge remained visible after masking");
      ++report.score.leakage_events;
      report.results.push_back(std::move(result));
      continue;
    }

    if (benchmark.id == "laplacian_rediscovery") {
      if (has_relation(hidden, "op.gradient", RelationKind::ComposesAfter, "op.divergence") ||
          std::any_of(structural.graph.begin(), structural.graph.end(), [](const auto& edge) {
            return edge.inner == "op.gradient" && edge.outer == "op.divergence";
          })) {
        mark_found(result, report.score, RediscoveryOutcome::StructuralEquivalent, "div ∘ grad",
                   "typed composition matches the hidden second-order Laplacian signature");
      } else {
        ++report.score.missed;
      }
    } else if (benchmark.id == "zero_composition") {
      const auto* gradient = hidden.find("op.gradient");
      const auto* curl = hidden.find("op.curl.3d");
      if (gradient && curl) {
        const Grid grid{3, 5, 5, 5, 0.1};
        const auto input = numerics::scalar_grid(grid, [](double x, double y, double z) { return x * y + z * z; });
        const auto first = numerics::NumericalExecutor{}.apply(gradient->id, input);
        const auto second = numerics::NumericalExecutor{}.apply(curl->id, first.output);
        const NumericObject expected{NumericObject::Kind::Vector, grid, 3,
                                     std::vector<double>(grid.nx * grid.ny * grid.nz * 3, 0.0)};
        if (second.supported && numerics::l2_error(second.output, expected) < 1e-6) {
          mark_found(result, report.score, RediscoveryOutcome::Partial, "curl ∘ grad ≈ 0",
                     "numerical annihilation was rediscovered on a finite-difference probe");
        } else {
          ++report.score.missed;
        }
      } else {
        ++report.score.missed;
      }
    } else if (benchmark.id == "chain_complex") {
      if (has_pattern(structural, patterns::PatternType::DifferentialComplexCandidate)) {
        mark_found(result, report.score, RediscoveryOutcome::StructuralEquivalent,
                   "abstract zero-composition chain", "typed differential sequence suggests a chain-complex candidate");
      } else {
        ++report.score.missed;
      }
    } else if (benchmark.id == "d_squared_abstraction") {
      const auto derivative = matching(analysis_atlas, "form.", "form.", 1, true, false);
      const auto zero = matching(analysis_atlas, "form.", "form.", 0, true, false);
      const bool primitives = !derivative.empty() && !zero.empty();
      const bool annihilator = !benchmark.strict_blind && has_relation(analysis_atlas, "form.exterior_derivative",
                                                                         RelationKind::Annihilates, "form.zero");
      if (primitives && annihilator) {
        mark_found(result, report.score, RediscoveryOutcome::StructuralEquivalent, "d ∘ d = 0",
                   "graded differential and zero target form an abstract nilpotent differential candidate");
      } else if (primitives) {
        mark_found(result, report.score, RediscoveryOutcome::Partial, "nilpotent differential candidate",
                   "the typed primitives support d-squared-zero, but no visible annihilation witness remains");
      } else {
        ++report.score.missed;
      }
    } else if (benchmark.id == "hodge_decomposition") {
      const auto hodge = matching(analysis_atlas, "form.", "form.", 0, true, false);
      const auto codifferential = matching(analysis_atlas, "form.", "form.", 1, true, false);
      const auto laplacian = matching(analysis_atlas, "form.", "form.", 2, true, false);
      const bool primitives = hodge.size() >= 2 && !codifferential.empty() && !laplacian.empty();
      const bool decomposition = !benchmark.strict_blind && has_relation(analysis_atlas, "form.de_rham_laplacian",
                                                                           RelationKind::Decomposition,
                                                                           "form.exterior_derivative");
      if (primitives && decomposition) {
        mark_found(result, report.score, RediscoveryOutcome::StructuralEquivalent,
                   "Hodge/Laplacian decomposition", "adjoint, Hodge-star, and Laplacian signatures form the expected decomposition family");
      } else if (primitives) {
        mark_found(result, report.score, RediscoveryOutcome::Partial, "Hodge decomposition candidate",
                   "Hodge primitives are present, while the explicit decomposition witness is hidden");
      } else {
        ++report.score.missed;
      }
    } else if (benchmark.id == "fourier_convolution") {
      const auto forward = matching(analysis_atlas, "signal.", "frequency.", 0, true, false);
      const auto inverse = matching(analysis_atlas, "frequency.", "signal.", 0, true, false);
      const auto kernel = matching(analysis_atlas, "signal.", "signal.", 0, true, true);
      const bool primitives = !forward.empty() && !inverse.empty() && !kernel.empty();
      const bool correspondence = !benchmark.strict_blind && has_relation(analysis_atlas, "transform.convolution",
                                                                            RelationKind::TransformCorrespondence,
                                                                            "transform.fourier");
      if (primitives && correspondence) {
        mark_found(result, report.score, RediscoveryOutcome::StructuralEquivalent,
                   "Fourier/convolution correspondence", "transform pair and bilinear kernel operator expose the convolution theorem shape");
      } else if (primitives) {
        mark_found(result, report.score, RediscoveryOutcome::Partial, "Fourier convolution candidate",
                   "transform primitives support the correspondence, but its explicit relation is hidden");
      } else {
        ++report.score.missed;
      }
    } else if (benchmark.id == "continuous_discrete_analogy") {
      const auto continuous_gradient = matching(analysis_atlas, "scalar.", "vector.", 1, true, false);
      const auto discrete_gradient = matching(analysis_atlas, "grid.scalar.", "grid.vector.", 1, false, true);
      const auto continuous_laplacian = matching(analysis_atlas, "scalar.", "scalar.", 2, true, false);
      const auto discrete_laplacian = matching(analysis_atlas, "grid.scalar.", "grid.scalar.", 2, false, true);
      const bool primitives = !continuous_gradient.empty() && !discrete_gradient.empty() &&
                               !continuous_laplacian.empty() && !discrete_laplacian.empty();
      const bool analogy = !benchmark.strict_blind && has_relation(analysis_atlas, "discrete.gradient",
                                                                     RelationKind::DiscreteAnalog, "op.gradient");
      if (primitives && analogy) {
        mark_found(result, report.score, RediscoveryOutcome::StructuralEquivalent,
                   "continuous/discrete differential analogy", "matching signatures and discrete counterparts form a cross-domain bridge");
      } else if (primitives) {
        mark_found(result, report.score, RediscoveryOutcome::Partial, "continuous/discrete analogy candidate",
                   "continuous and discrete differential primitives share a typed shape");
      } else {
        ++report.score.missed;
      }
    } else if (benchmark.id == "projection_decomposition") {
      const auto projections = matching(analysis_atlas, "matrix.operator", "matrix.operator", 0, true, true);
      const auto matrix_to_matrix = matching(analysis_atlas, "matrix.", "matrix.", 0, true, true);
      const bool primitives = projections.size() >= 2 && matrix_to_matrix.size() >= 4;
      const bool decomposition = !benchmark.strict_blind && has_relation(analysis_atlas, "la.symmetric_projection",
                                                                          RelationKind::Decomposition,
                                                                          "la.skew_projection");
      if (primitives && decomposition) {
        mark_found(result, report.score, RediscoveryOutcome::StructuralEquivalent,
                   "projection/decomposition family", "projection, orthogonal projection, and symmetric/skew operators expose a decomposition lattice");
      } else if (primitives) {
        mark_found(result, report.score, RediscoveryOutcome::Partial, "projection decomposition candidate",
                   "projection primitives are typed consistently, but the explicit decomposition relation is hidden");
      } else {
        ++report.score.missed;
      }
    } else {
      ++report.score.missed;
      result.explanation = "benchmark case is not registered; no discovery is credited";
    }

    report.results.push_back(std::move(result));
  }

  const auto found = report.score.exact + report.score.structural + report.score.partial;
  std::set<std::string> unique_discoveries;
  for (const auto& result : report.results) {
    for (const auto& discovery : result.discoveries) {
      if (!unique_discoveries.insert(discovery).second) ++report.score.duplicate_abstractions;
    }
  }
  report.score.recall = cases.empty() ? 0.0 : static_cast<double>(found) / static_cast<double>(cases.size());
  report.score.precision = found + report.score.false_positive == 0
                               ? 0.0
                               : static_cast<double>(found) /
                                     static_cast<double>(found + report.score.false_positive);
  report.score.false_discovery_rate = 1.0 - report.score.precision;
  return report;
}

std::string HistoricalRediscovery::export_text(const RediscoveryReport& report) const {
  std::ostringstream out;
  out << "Rediscovery score\n"
      << "Exact: " << report.score.exact << "\n"
      << "Structural: " << report.score.structural << "\n"
      << "Partial: " << report.score.partial << "\n"
      << "Missed: " << report.score.missed << "\n"
      << "False positives: " << report.score.false_positive << "\n"
      << "Leakage events: " << report.score.leakage_events << "\n"
      << "Total patterns: " << report.score.total_patterns << "\n"
      << "Accepted matches: " << report.score.accepted_matches << "\n"
      << "Duplicate abstractions: " << report.score.duplicate_abstractions << "\n"
      << "Weak families: " << report.score.weak_families << "\n"
      << "Precision: " << report.score.precision << "\n"
      << "Recall: " << report.score.recall << "\n";
  for (const auto& result : report.results) {
    out << result.benchmark_id << ": " << to_string(result.outcome) << " — " << result.explanation << "\n";
  }
  return out.str();
}

std::string HistoricalRediscovery::export_json(const RediscoveryReport& report) const {
  std::ostringstream out;
  out << "{\"exact\":" << report.score.exact
      << ",\"structural\":" << report.score.structural
      << ",\"partial\":" << report.score.partial
      << ",\"missed\":" << report.score.missed
      << ",\"false_positive\":" << report.score.false_positive
      << ",\"leakage_events\":" << report.score.leakage_events
      << ",\"total_patterns\":" << report.score.total_patterns
      << ",\"accepted_matches\":" << report.score.accepted_matches
      << ",\"duplicate_abstractions\":" << report.score.duplicate_abstractions
      << ",\"weak_families\":" << report.score.weak_families
      << ",\"precision\":" << report.score.precision
      << ",\"recall\":" << report.score.recall
      << ",\"false_discovery_rate\":" << report.score.false_discovery_rate << "}";
  return out.str();
}

}  // namespace opforge::benchmarks
