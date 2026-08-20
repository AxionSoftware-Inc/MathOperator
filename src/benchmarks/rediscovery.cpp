#include "opforge/benchmarks/rediscovery.hpp"
#include "opforge/atlas/seed.hpp"
#include "opforge/research/campaign.hpp"
#include "opforge/synthesis/candidate.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
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

const char* to_string(BlindOutcome outcome) {
  switch (outcome) {
    case BlindOutcome::Exact: return "exact";
    case BlindOutcome::StructuralEquivalent: return "structural_equivalent";
    case BlindOutcome::Partial: return "partial";
    case BlindOutcome::Missed: return "missed";
    case BlindOutcome::FalsePositive: return "false_positive";
    case BlindOutcome::NegativeControlPass: return "negative_control_pass";
    case BlindOutcome::InvalidBenchmark: return "invalid_benchmark";
  }
  return "unknown";
}

namespace {

struct MaskedFixture {
  atlas::Atlas atlas;
  std::map<std::string, std::string> source_to_blind;
  int leakage_events{0};
};

bool references_any(const atlas::ExpressionPtr& expression, const std::set<std::string>& ids) {
  if (!expression) return false;
  if (expression->kind == atlas::Expression::Kind::OperatorReference && ids.contains(expression->value)) return true;
  return std::any_of(expression->children.begin(), expression->children.end(),
                     [&](const auto& child) { return references_any(child, ids); });
}

atlas::ExpressionPtr remap_expression(const atlas::ExpressionPtr& expression,
                                       const std::map<std::string, std::string>& ids) {
  if (!expression) return nullptr;
  auto value = expression->value;
  if (expression->kind == atlas::Expression::Kind::OperatorReference) {
    const auto it = ids.find(value);
    if (it == ids.end()) return nullptr;
    value = it->second;
  }
  std::vector<atlas::ExpressionPtr> children;
  children.reserve(expression->children.size());
  for (const auto& child : expression->children) {
    auto mapped = remap_expression(child, ids);
    if (child && !mapped) return nullptr;
    children.push_back(std::move(mapped));
  }
  return std::make_shared<atlas::Expression>(expression->kind, std::move(value), std::move(children));
}

std::string relation_key(const std::string& source, atlas::RelationKind kind, const std::string& target) {
  return source + "|" + atlas::to_string(kind) + "|" + target;
}

bool hidden_relation(const std::set<std::string>& hidden, const std::string& source,
                     const atlas::OperatorRelation& relation) {
  return hidden.contains(relation_key(source, relation.kind, relation.target_id)) ||
         hidden.contains(source + "->" + relation.target_id);
}

bool target_pair_relation(const BlindRediscoveryCase& benchmark, const std::string& source,
                          const std::string& target) {
  if (benchmark.target.kind == BlindTarget::Kind::NoForbiddenComposition) return false;
  return (source == benchmark.target.first_operator && target == benchmark.target.second_operator) ||
         (source == benchmark.target.second_operator && target == benchmark.target.first_operator);
}

MaskedFixture mask_for_blind_search(const atlas::Atlas& source, const BlindRediscoveryCase& benchmark) {
  MaskedFixture fixture;
  const auto all = source.all();
  int operator_index = 0;
  for (const auto* op : all) {
    if (!benchmark.hidden_operator_ids.contains(op->id))
      fixture.source_to_blind[op->id] = "blind.op." + std::to_string(operator_index++);
  }

  int space_index = 0;
  for (const auto& original : source.spaces()) {
    auto space = original;
    space.name = "space_" + std::to_string(space_index++);
    fixture.atlas.add_space(std::move(space));
  }

  for (const auto* original : all) {
    if (benchmark.hidden_operator_ids.contains(original->id)) continue;
    const auto mapped = fixture.source_to_blind.at(original->id);
    auto op = *original;
    op.id = mapped;
    op.name = "operator_" + std::to_string(operator_index++);
    op.symbol.clear();
    op.mathematical_domain.clear();
    op.provenance_category.clear();
    op.aliases.clear();
    op.parameters.clear();
    op.invariants.clear();
    op.theorems.clear();
    op.applications.clear();
    op.limitations.clear();
    op.sources.clear();
    op.coordinate_definition.clear();
    op.coordinate_free_definition.clear();
    op.discrete_definition.clear();
    op.numerical_stability.clear();
    op.complexity.clear();
    op.evidence.clear();
    op.verification = atlas::VerificationStatus::Proposed;
    op.definition = references_any(original->definition, benchmark.hidden_operator_ids)
                        ? nullptr
                        : remap_expression(original->definition, fixture.source_to_blind);
    op.relations.clear();
    for (const auto& original_relation : original->relations) {
      if (benchmark.hidden_operator_ids.contains(original_relation.target_id) ||
          hidden_relation(benchmark.hidden_relation_keys, original->id, original_relation) ||
          target_pair_relation(benchmark, original->id, original_relation.target_id)) continue;
      const auto target = fixture.source_to_blind.find(original_relation.target_id);
      if (target == fixture.source_to_blind.end()) continue;
      op.relations.push_back({original_relation.kind, target->second, {}, {}});
    }
    fixture.atlas.add(std::move(op));
  }

  int identity_index = 0;
  for (const auto& original : source.identities()) {
    if (benchmark.hidden_identity_ids.contains(original.id)) continue;
    if (references_any(original.left, benchmark.hidden_operator_ids) ||
        references_any(original.right, benchmark.hidden_operator_ids)) continue;
    const bool target_pair_statement =
        benchmark.target.kind != BlindTarget::Kind::NoForbiddenComposition &&
        ((references_any(original.left, {benchmark.target.first_operator}) &&
          references_any(original.right, {benchmark.target.second_operator})) ||
         (references_any(original.left, {benchmark.target.second_operator}) &&
          references_any(original.right, {benchmark.target.first_operator})));
    if (target_pair_statement) continue;
    auto identity = original;
    identity.id = "blind.identity." + std::to_string(identity_index++);
    identity.name = "visible_statement";
    identity.assumptions.clear();
    identity.dimension_constraints.clear();
    identity.regularity_constraints.clear();
    identity.counterexamples.clear();
    identity.sources.clear();
    identity.required_structures.clear();
    identity.applicable_domains.clear();
    identity.canonical_form.clear();
    identity.provenance_category.clear();
    identity.metric.clear();
    identity.orientation.clear();
    identity.boundary.clear();
    identity.scalar_field.clear();
    identity.object_grade.clear();
    identity.curvature.clear();
    identity.geometry_regime.clear();
    identity.evidence.clear();
    identity.verification = atlas::VerificationStatus::Proposed;
    identity.left = remap_expression(original.left, fixture.source_to_blind);
    identity.right = remap_expression(original.right, fixture.source_to_blind);
    if (!identity.left || !identity.right) continue;
    fixture.atlas.add_identity(std::move(identity));
  }

  for (const auto& hidden_id : benchmark.hidden_identity_ids)
    if (fixture.atlas.find_identity(hidden_id)) ++fixture.leakage_events;
  for (const auto& hidden_id : benchmark.hidden_operator_ids)
    if (fixture.atlas.find(hidden_id)) ++fixture.leakage_events;
  for (const auto* op : fixture.atlas.all()) {
    for (const auto& relation : op->relations) {
      if (relation.target_id.rfind("blind.op.", 0) != 0) ++fixture.leakage_events;
    }
  }
  return fixture;
}

bool contains_operator_pair(const patterns::StructuralPattern& pattern,
                            const std::string& first, const std::string& second) {
  return std::find(pattern.operators.begin(), pattern.operators.end(), first) != pattern.operators.end() &&
         std::find(pattern.operators.begin(), pattern.operators.end(), second) != pattern.operators.end();
}

BlindRediscoveryResult score_blind_result(const BlindRediscoveryCase& benchmark,
                                          const MaskedFixture& fixture,
                                          const patterns::PatternReport& patterns,
                                          const synthesis::CandidateReport& candidates) {
  // This is the external benchmark scorer. The masked discovery path above
  // receives no BlindTarget and only produces generic structural observations.
  BlindRediscoveryResult result;
  result.benchmark_id = benchmark.id;
  result.description = benchmark.description;
  {
    std::vector<std::string> ids;
    ids.reserve(candidates.accepted.size() + candidates.rejected.size());
    for (const auto& candidate : candidates.accepted) ids.push_back(candidate.id);
    for (const auto& candidate : candidates.rejected) ids.push_back(candidate.id);
    std::sort(ids.begin(), ids.end());
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& id : ids) {
      for (const unsigned char byte : id) {
        hash ^= byte;
        hash *= 1099511628211ULL;
      }
      hash ^= 0xffU;
      hash *= 1099511628211ULL;
    }
    std::ostringstream digest;
    digest << std::hex << hash;
    result.candidate_id_digest = digest.str();
  }
  result.graph_candidates = patterns.graph_candidates;
  result.compatible_edges = patterns.graph.size();
  result.pattern_candidates = patterns.pattern_candidates;
  result.retained_patterns = patterns.patterns.size();
  result.generated_candidates = candidates.raw_candidates;
  result.canonical_classes = candidates.accepted.size() + candidates.rejected.size();
  result.canonical_duplicates = candidates.duplicate_candidates;
  result.rejected_candidates = candidates.rejected.size();
  result.truncated = patterns.truncated;
  result.numerical_experiments = 0;

  const auto first = fixture.source_to_blind.find(benchmark.target.first_operator);
  const auto second = fixture.source_to_blind.find(benchmark.target.second_operator);
  const bool endpoints_visible = first != fixture.source_to_blind.end() && second != fixture.source_to_blind.end();
  const bool composition_found = endpoints_visible && std::any_of(
      patterns.graph.begin(), patterns.graph.end(), [&](const auto& edge) {
        return edge.inner == first->second && edge.outer == second->second;
      });
  const bool zero_found = endpoints_visible && std::any_of(
      patterns.patterns.begin(), patterns.patterns.end(), [&](const auto& pattern) {
        return pattern.type == patterns::PatternType::ZeroComposition &&
               pattern.operators.size() == 2 && pattern.operators[0] == first->second &&
               pattern.operators[1] == second->second;
      });
  const bool family_found = endpoints_visible && std::any_of(
      patterns.patterns.begin(), patterns.patterns.end(), [&](const auto& pattern) {
        return pattern.type == patterns::PatternType::OperatorFamily &&
               contains_operator_pair(pattern, first->second, second->second);
      });

  if (benchmark.negative_control || benchmark.target.kind == BlindTarget::Kind::NoForbiddenComposition) {
    if (composition_found) {
      result.outcome = BlindOutcome::FalsePositive;
      result.explanation = "forbidden type/near-miss composition was emitted by the structural graph";
    } else {
      result.outcome = BlindOutcome::NegativeControlPass;
      result.explanation = "the forbidden construction was not emitted";
    }
  } else if (benchmark.target.kind == BlindTarget::Kind::Composition) {
    if (composition_found) {
      result.outcome = BlindOutcome::StructuralEquivalent;
      result.discoveries.push_back("compose(" + second->second + "," + first->second + ")");
      result.explanation = "the hidden fact was matched by an external scorer against a visible typed composition";
    } else {
      result.outcome = BlindOutcome::Missed;
      result.explanation = "no target-shaped typed composition survived the blind mask";
    }
  } else if (benchmark.target.kind == BlindTarget::Kind::ZeroComposition) {
    if (zero_found) {
      result.outcome = BlindOutcome::Exact;
      result.discoveries.push_back("zero_compose(" + second->second + "," + first->second + ")");
      result.explanation = "a zero-composition pattern was recovered without numerical execution";
    } else if (composition_found) {
      result.outcome = BlindOutcome::Partial;
      result.discoveries.push_back("compose(" + second->second + "," + first->second + ")");
      result.explanation = "the composition was visible, but the hidden zero property was not recovered";
    } else {
      result.outcome = BlindOutcome::Missed;
      result.explanation = "neither the target composition nor its zero property was recovered";
    }
  } else if (benchmark.target.kind == BlindTarget::Kind::Relation) {
    if (family_found) {
      result.outcome = BlindOutcome::Partial;
      result.discoveries.push_back("family(" + first->second + "," + second->second + ")");
      result.explanation = "endpoint structure remained visible, but the hidden relation itself was not recovered";
    } else {
      result.outcome = BlindOutcome::Missed;
      result.explanation = "the hidden relation was not recovered from visible structural data";
    }
  }
  return result;
}

void accumulate_blind(BlindRediscoveryScorecard& score, const BlindRediscoveryResult& result) {
  score.graph_candidates += result.graph_candidates;
  score.compatible_edges += result.compatible_edges;
  score.patterns += result.retained_patterns;
  score.generated_candidates += result.generated_candidates;
  score.canonical_classes += result.canonical_classes;
  score.canonical_duplicates += result.canonical_duplicates;
  score.rejected_candidates += result.rejected_candidates;
  score.numerical_experiments += result.numerical_experiments;
  switch (result.outcome) {
    case BlindOutcome::Exact: ++score.exact; break;
    case BlindOutcome::StructuralEquivalent: ++score.structural; break;
    case BlindOutcome::Partial: ++score.partial; break;
    case BlindOutcome::Missed: ++score.missed; break;
    case BlindOutcome::FalsePositive: ++score.false_positive; break;
    case BlindOutcome::NegativeControlPass: ++score.negative_control_pass; break;
    case BlindOutcome::InvalidBenchmark: ++score.invalid_benchmarks; break;
  }
}

atlas::Atlas subset_atlas(const atlas::Atlas& source, size_t limit) {
  const auto all = source.all();
  std::set<std::string> kept;
  for (size_t i = 0; i < std::min(limit, all.size()); ++i) kept.insert(all[i]->id);
  std::set<std::string> hidden;
  for (const auto* op : all) if (!kept.contains(op->id)) hidden.insert(op->id);
  atlas::Atlas subset;
  for (const auto& space : source.spaces()) subset.add_space(space);
  for (const auto* original : all) {
    if (!kept.contains(original->id)) continue;
    auto op = *original;
    op.relations.erase(std::remove_if(op.relations.begin(), op.relations.end(),
                                      [&](const auto& relation) { return !kept.contains(relation.target_id); }),
                       op.relations.end());
    subset.add(std::move(op));
  }
  for (const auto& original : source.identities()) {
    if (references_any(original.left, hidden) || references_any(original.right, hidden)) continue;
    subset.add_identity(original);
  }
  return subset;
}

std::vector<std::string> canonical_classes(const synthesis::CandidateReport& report) {
  std::vector<std::string> result;
  result.reserve(report.accepted.size() + report.rejected.size());
  for (const auto& candidate : report.accepted) result.push_back(candidate.canonical_form);
  for (const auto& candidate : report.rejected) result.push_back(candidate.canonical_form);
  return result;
}

}  // namespace

BlindRediscoveryReport BlindRediscoveryHarness::run(
    const atlas::Atlas& source, const std::vector<BlindRediscoveryCase>& cases) const {
  BlindRediscoveryReport report;
  report.engine_knows_targets = false;
  for (const auto& benchmark : cases) {
    const auto fixture = mask_for_blind_search(source, benchmark);
    BlindRediscoveryResult result;
    if (fixture.leakage_events > 0) {
      result.benchmark_id = benchmark.id;
      result.description = benchmark.description;
      result.outcome = BlindOutcome::InvalidBenchmark;
      result.explanation = "declared hidden target information remained in the masked Atlas";
      report.score.leakage_events += fixture.leakage_events;
      ++report.score.invalid_benchmarks;
      report.results.push_back(std::move(result));
      continue;
    }
    const auto structural = patterns::PatternAnalyzer{}.analyze(fixture.atlas, benchmark.pattern_budget);
    const auto candidates = synthesis::CandidateSynthesizer{}.synthesize(fixture.atlas, structural);
    result = score_blind_result(benchmark, fixture, structural, candidates);
    accumulate_blind(report.score, result);
    report.results.push_back(std::move(result));
  }
  const int positive_cases = report.score.exact + report.score.structural + report.score.partial + report.score.missed;
  const int positive_hits = report.score.exact + report.score.structural + report.score.partial;
  const int negative_controls = report.score.negative_control_pass + report.score.false_positive;
  report.score.exact_rate = positive_cases == 0 ? 0.0 : static_cast<double>(report.score.exact) / positive_cases;
  report.score.structural_rate = positive_cases == 0 ? 0.0 : static_cast<double>(report.score.structural) / positive_cases;
  report.score.structural_recovery_rate = positive_cases == 0 ? 0.0 : static_cast<double>(report.score.exact + report.score.structural) / positive_cases;
  report.score.partial_rate = positive_cases == 0 ? 0.0 : static_cast<double>(report.score.partial) / positive_cases;
  report.score.miss_rate = positive_cases == 0 ? 0.0 : static_cast<double>(report.score.missed) / positive_cases;
  report.score.false_positive_rate = negative_controls == 0 ? 0.0 : static_cast<double>(report.score.false_positive) / negative_controls;
  report.score.negative_control_pass_rate = negative_controls == 0 ? 0.0 : static_cast<double>(report.score.negative_control_pass) / negative_controls;
  report.score.useful_signal_rate = positive_cases == 0 ? 0.0 : static_cast<double>(positive_hits) / positive_cases;
  report.score.recall = report.score.structural_recovery_rate;
  report.score.precision = report.score.exact + report.score.structural + report.score.false_positive == 0
                               ? 0.0
                               : static_cast<double>(report.score.exact + report.score.structural) /
                                     static_cast<double>(report.score.exact + report.score.structural + report.score.false_positive);
  report.score.false_discovery_rate = 1.0 - report.score.precision;
  return report;
}

std::string BlindRediscoveryHarness::export_text(const BlindRediscoveryReport& report) const {
  std::ostringstream out;
  out << "Target-blind rediscovery regression\n"
      << "Engine knows targets: " << (report.engine_knows_targets ? "yes" : "no") << "\n"
      << "Exact: " << report.score.exact << "\n"
      << "Structural: " << report.score.structural << "\n"
      << "Partial: " << report.score.partial << "\n"
      << "Missed: " << report.score.missed << "\n"
      << "False positives: " << report.score.false_positive << "\n"
      << "Negative controls passed: " << report.score.negative_control_pass << "\n"
      << "Invalid benchmarks: " << report.score.invalid_benchmarks << "\n"
      << "Leakage events: " << report.score.leakage_events << "\n"
      << "Graph candidates: " << report.score.graph_candidates << "\n"
      << "Compatible edges: " << report.score.compatible_edges << "\n"
      << "Retained patterns: " << report.score.patterns << "\n"
      << "Generated candidates: " << report.score.generated_candidates << "\n"
      << "Canonical classes: " << report.score.canonical_classes << "\n"
      << "Canonical duplicates: " << report.score.canonical_duplicates << "\n"
      << "Rejected candidates: " << report.score.rejected_candidates << "\n"
      << "Numerical experiments: " << report.score.numerical_experiments << "\n"
      << "Exact rate: " << report.score.exact_rate << "\n"
      << "Structural rate: " << report.score.structural_rate << "\n"
      << "Full structural recovery rate: " << report.score.structural_recovery_rate << "\n"
      << "Partial rate: " << report.score.partial_rate << "\n"
      << "Miss rate: " << report.score.miss_rate << "\n"
      << "False-positive rate: " << report.score.false_positive_rate << "\n"
      << "Negative-control pass rate: " << report.score.negative_control_pass_rate << "\n"
      << "Useful signal rate (includes partial): " << report.score.useful_signal_rate << "\n"
      << "Precision: " << report.score.precision << "\n"
      << "Deprecated recall compatibility field: " << report.score.recall << "\n";
  for (const auto& result : report.results) {
    out << result.benchmark_id << ": " << to_string(result.outcome) << " — " << result.explanation << "\n";
    out << "  candidate_id_digest: " << result.candidate_id_digest << "\n";
    for (const auto& discovery : result.discoveries) out << "  discovery: " << discovery << "\n";
  }
  return out.str();
}

std::string BlindRediscoveryHarness::export_json(const BlindRediscoveryReport& report) const {
  std::ostringstream out;
  out << "{\"engine_knows_targets\":" << (report.engine_knows_targets ? "true" : "false")
      << ",\"exact\":" << report.score.exact
      << ",\"structural\":" << report.score.structural
      << ",\"partial\":" << report.score.partial
      << ",\"missed\":" << report.score.missed
      << ",\"false_positive\":" << report.score.false_positive
      << ",\"negative_control_pass\":" << report.score.negative_control_pass
      << ",\"invalid_benchmarks\":" << report.score.invalid_benchmarks
      << ",\"leakage_events\":" << report.score.leakage_events
      << ",\"generated_candidates\":" << report.score.generated_candidates
      << ",\"canonical_classes\":" << report.score.canonical_classes
      << ",\"canonical_duplicates\":" << report.score.canonical_duplicates
      << ",\"numerical_experiments\":" << report.score.numerical_experiments
      << ",\"exact_rate\":" << report.score.exact_rate
      << ",\"structural_rate\":" << report.score.structural_rate
      << ",\"structural_recovery_rate\":" << report.score.structural_recovery_rate
      << ",\"partial_rate\":" << report.score.partial_rate
      << ",\"miss_rate\":" << report.score.miss_rate
      << ",\"false_positive_rate\":" << report.score.false_positive_rate
      << ",\"negative_control_pass_rate\":" << report.score.negative_control_pass_rate
      << ",\"useful_signal_rate\":" << report.score.useful_signal_rate
      << ",\"precision\":" << report.score.precision
      << ",\"recall\":" << report.score.recall << "}";
  return out.str();
}

ScalingReport ScalingRegression::run(const atlas::Atlas& source, size_t medium_operator_count) const {
  const std::vector<std::pair<std::string, atlas::Atlas>> fixtures = {
      {"small-seed", atlas::make_vector_calculus_seed()},
      {"new-approximately-50-operator-subset", subset_atlas(source, medium_operator_count)},
      {"full-atlas", source}};
  ScalingReport report;
  for (const auto& [label, fixture] : fixtures) {
    const auto start = std::chrono::steady_clock::now();
    patterns::PatternBudget budget;
    budget.max_composition_checks = 200000;
    budget.max_graph_edges = 20000;
    budget.max_patterns = 20000;
    const auto structural = patterns::PatternAnalyzer{}.analyze(fixture, budget);
    const auto candidates = synthesis::CandidateSynthesizer{}.synthesize(fixture, structural);
    const auto classes = canonical_classes(candidates);
    research::CampaignConfig config;
    config.campaign_id = "C-scaling-" + label;
    config.atlas_snapshot = "scientific-regression-v1-" + label;
    config.mode = research::CampaignMode::StructuralExploration;
    config.budget = {1, 40, 0, 60000};
    config.pattern_budget = budget;
    config.max_candidate_leads = 64;
    config.enable_numerical_verification = false;
    config.run_numeric_diagnostics = false;
    const auto campaign = research::ResearchOrchestrator{}.run(fixture, config);
    ScalingRun run;
    run.id = "scale-" + label;
    run.label = label;
    run.operators = fixture.all().size();
    run.spaces = fixture.spaces().size();
    run.compatible_edges = structural.graph.size();
    run.raw_candidate_count = candidates.raw_candidates;
    run.canonical_classes = classes.size();
    run.canonical_duplicates = candidates.duplicate_candidates;
    run.rejected_candidates = candidates.rejected.size();
    run.pruned_candidates = campaign.pruned_candidates;
    run.maximum_retained_frontier = config.max_candidate_leads;
    run.numerical_experiments = campaign.numerical_experiments;
    run.truncated = structural.truncated || campaign.pruned_candidates > 0;
    run.runtime_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    report.runs.push_back(std::move(run));
  }
  return report;
}

std::string ScalingRegression::export_text(const ScalingReport& report) const {
  std::ostringstream out;
  out << "Search-explosion scaling regression\n";
  for (const auto& run : report.runs) {
    out << run.id << " label=" << run.label
        << " operators=" << run.operators
        << " spaces=" << run.spaces
        << " compatible_edges=" << run.compatible_edges
        << " raw_candidates=" << run.raw_candidate_count
        << " canonical_classes=" << run.canonical_classes
        << " canonical_duplicates=" << run.canonical_duplicates
        << " rejected=" << run.rejected_candidates
        << " pruned=" << run.pruned_candidates
        << " maximum_retained_frontier=" << run.maximum_retained_frontier
        << " runtime_ms=" << run.runtime_ms
        << " numerical_experiments=" << run.numerical_experiments
        << " truncated=" << (run.truncated ? "yes" : "no") << "\n";
  }
  return out.str();
}

std::string ScalingRegression::export_json(const ScalingReport& report) const {
  std::ostringstream out;
  out << "{\"runs\":[";
  for (size_t i = 0; i < report.runs.size(); ++i) {
    if (i) out << ',';
    const auto& run = report.runs[i];
    out << "{\"id\":\"" << run.id << "\",\"operators\":" << run.operators
        << ",\"spaces\":" << run.spaces
        << ",\"compatible_edges\":" << run.compatible_edges
        << ",\"raw_candidates\":" << run.raw_candidate_count
        << ",\"canonical_classes\":" << run.canonical_classes
        << ",\"canonical_duplicates\":" << run.canonical_duplicates
        << ",\"rejected\":" << run.rejected_candidates
        << ",\"pruned\":" << run.pruned_candidates
        << ",\"maximum_retained_frontier\":" << run.maximum_retained_frontier
        << ",\"runtime_ms\":" << run.runtime_ms
        << ",\"numerical_experiments\":" << run.numerical_experiments
        << ",\"truncated\":" << (run.truncated ? "true" : "false") << "}";
  }
  out << "]}";
  return out.str();
}

std::vector<BlindRediscoveryCase> default_blind_rediscovery_cases() {
  using Kind = atlas::RelationKind;
  const auto key = [](const std::string& source, Kind kind, const std::string& target) {
    return relation_key(source, kind, target);
  };
  std::vector<BlindRediscoveryCase> cases;
  cases.push_back({"blind_laplacian_composition", "Laplacian-like composition with target operator and identities hidden",
                   {"id.vector.grad_div_laplacian", "atlas.identity.laplacian"},
                   {key("op.gradient", Kind::Factorization, "op.laplacian"),
                    key("op.divergence", Kind::Factorization, "op.laplacian")},
                   {"op.laplacian"},
                   {BlindTarget::Kind::Composition, "op.gradient", "op.divergence", Kind::RelatedTo}});
  cases.push_back({"blind_curl_gradient_zero", "Zero-composition fact with its identity hidden",
                   {"id.vector.curl_grad_zero"}, {}, {},
                   {BlindTarget::Kind::ZeroComposition, "op.gradient", "op.curl.3d", Kind::RelatedTo}});
  cases.push_back({"blind_exterior_nilpotence", "Nilpotence-style fact with duplicate identities hidden",
                   {"id.forms.d_squared", "atlas.identity.exterior_square"}, {}, {},
                   {BlindTarget::Kind::ZeroComposition, "form.exterior_derivative", "form.exterior_derivative", Kind::RelatedTo}});
  cases.push_back({"blind_projection_decomposition", "Decomposition relation and semantic identity hidden",
                   {"id.linear.symmetric_skew"}, {key("la.symmetric_projection", Kind::Decomposition, "la.skew_projection")}, {},
                   {BlindTarget::Kind::Relation, "la.symmetric_projection", "la.skew_projection", Kind::Decomposition}});
  cases.push_back({"blind_fourier_correspondence", "Transform correspondence hidden from the structural search",
                   {"id.transform.convolution_theorem"}, {key("transform.convolution", Kind::TransformCorrespondence, "transform.fourier")}, {},
                   {BlindTarget::Kind::Relation, "transform.convolution", "transform.fourier", Kind::TransformCorrespondence}});
  cases.push_back({"blind_continuous_discrete_analogy", "Continuous/discrete bridge hidden",
                   {"id.discrete.continuous_gradient"}, {key("op.gradient", Kind::DiscreteAnalog, "discrete.gradient")}, {},
                   {BlindTarget::Kind::Relation, "op.gradient", "discrete.gradient", Kind::DiscreteAnalog}});
  cases.push_back({"negative_type_incompatible", "Gradient composed with gradient must not be emitted",
                   {}, {}, {},
                   {BlindTarget::Kind::NoForbiddenComposition, "op.gradient", "op.gradient", Kind::RelatedTo}, true});
  cases.push_back({"negative_near_miss", "Divergence composed with divergence must not be emitted",
                   {}, {}, {},
                   {BlindTarget::Kind::NoForbiddenComposition, "op.divergence", "op.divergence", Kind::RelatedTo}, true});
  cases.push_back({"negative_missing_prerequisite", "A hidden divergence prerequisite must prevent the composition",
                   {}, {}, {"op.divergence"},
                   {BlindTarget::Kind::NoForbiddenComposition, "op.gradient", "op.divergence", Kind::RelatedTo}, true});
  return cases;
}

}  // namespace opforge::benchmarks
