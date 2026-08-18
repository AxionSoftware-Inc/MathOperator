#include "opforge/patterns/meta.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace opforge::patterns {
namespace {

std::string domain_of(const atlas::Atlas& atlas, const StructuralPattern& pattern) {
  for (const auto& id : pattern.operators) {
    if (const auto* op = atlas.find(id)) {
      if (const auto* space = atlas.find_space(op->signature.domain.id))
        return space->base_domain.empty() ? space->id : space->base_domain;
      return op->signature.domain.id;
    }
  }
  return "unknown";
}

PatternObject object_of(const atlas::Atlas& atlas, const StructuralPattern& pattern) {
  PatternObject object;
  object.id = pattern.id;
  object.type = pattern.type;
  object.canonical_law = to_string(pattern.type);
  object.identities = pattern.derived_relations;
  object.assumptions = pattern.assumptions;
  object.evidence = pattern.evidence;
  object.concrete_realizations = pattern.operators;
  object.participating_domains.push_back(domain_of(atlas, pattern));
  for (const auto& op_id : pattern.operators) {
    if (const auto* op = atlas.find(op_id)) {
      object.role_graph.push_back(op->signature.domain.id + " -> " + op->signature.codomain.id);
      object.signature.input_role = op->signature.domain.id;
      object.signature.output_role = op->signature.codomain.id;
      object.signature.differential_order = std::max(object.signature.differential_order,
                                                      op->signature.differential_order);
      object.signature.linear = object.signature.linear && op->signature.linear;
      object.signature.local = object.signature.local && op->signature.local;
      object.signature.base_domain = domain_of(atlas, pattern);
    }
  }
  if (pattern.type == PatternType::ZeroComposition) {
    object.zero_relations = pattern.derived_relations;
    object.canonical_law = "next ∘ previous = 0";
  } else if (pattern.type == PatternType::SharedStructure) {
    object.canonical_law = "continuous_or_discrete_analogue";
  } else if (pattern.type == PatternType::MissingLinkCandidate) {
    object.failure_modes.push_back("terminal role is not realized");
  }
  return object;
}

void add_unique(std::vector<std::string>& values, const std::string& value) {
  if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

double score_family(const MetaPattern& meta) {
  const auto domains = std::min(1.0, meta.participating_domains.size() / 3.0);
  const auto realizations = std::min(1.0, meta.concrete_realizations.size() / 5.0);
  const auto assumption_generality = meta.assumptions.empty() ? 1.0 : 0.75;
  const auto compression = meta.member_pattern_ids.size() > 1
                               ? std::min(1.0, meta.member_pattern_ids.size() / 6.0)
                               : 0.0;
  return 0.35 * domains + 0.25 * realizations + 0.20 * assumption_generality + 0.20 * compression;
}

}  // namespace

MetaPatternReport MetaPatternAnalyzer::analyze(const atlas::Atlas& atlas, const PatternReport& report) const {
  MetaPatternReport result;
  for (const auto& pattern : report.patterns) result.objects.push_back(object_of(atlas, pattern));

  std::map<std::string, std::vector<const PatternObject*>> groups;
  for (const auto& object : result.objects) groups[object.canonical_law].push_back(&object);
  int meta_index = 1;
  for (const auto& [law, members] : groups) {
    if (members.size() < 2) continue;
    MetaPattern meta;
    meta.id = "MP-" + std::to_string(meta_index++);
    meta.law = law == "next ∘ previous = 0" ? "two-step complex law" : law;
    meta.canonical_law = law;
    for (const auto* member : members) {
      meta.member_pattern_ids.push_back(member->id);
      for (const auto& domain : member->participating_domains) add_unique(meta.participating_domains, domain);
      for (const auto& realization : member->concrete_realizations) add_unique(meta.concrete_realizations, realization);
      for (const auto& assumption : member->assumptions) add_unique(meta.assumptions, assumption);
    }
    meta.independent_realizations = static_cast<int>(meta.participating_domains.size());
    meta.family_score = score_family(meta);
    meta.reasons.push_back(std::to_string(meta.independent_realizations) + " independent domain realizations");
    meta.reasons.push_back(std::to_string(meta.concrete_realizations.size()) + " concrete realizations");
    meta.reasons.push_back("compression of " + std::to_string(meta.member_pattern_ids.size()) + " base patterns");
    if (meta.canonical_law == "next ∘ previous = 0") meta.predicted_roles.push_back("next operator after a typed differential role");
    result.meta_patterns.push_back(std::move(meta));
  }

  for (const auto& meta : result.meta_patterns) {
    if (meta.independent_realizations < 2) continue;
    if (meta.canonical_law != "next ∘ previous = 0" &&
        meta.canonical_law != "continuous_or_discrete_analogue") continue;
    int prediction_index = 1;
    for (const auto& object : result.objects) {
      if (object.type != PatternType::MissingLinkCandidate) continue;
      PatternPrediction prediction;
      prediction.id = meta.id + ".prediction." + std::to_string(prediction_index++);
      prediction.source_meta_pattern = meta.id;
      prediction.predicted_role = "terminal continuation of " + object.signature.output_role;
      prediction.goal = "missing_role_completion justified by " + meta.law;
      prediction.expected_signature = object.signature;
      prediction.expected_signature.input_role = object.signature.output_role;
      prediction.expected_signature.output_role = "predicted.codomain." + meta.id;
      prediction.expected_identities.push_back(meta.canonical_law);
      prediction.expected_assumptions = meta.assumptions;
      prediction.confidence_reasons = meta.reasons;
      prediction.confidence = std::min(0.95, 0.45 + meta.family_score * 0.5);
      prediction.justified = true;
      if (result.predictions.size() >= 32) return result;
      result.predictions.push_back(std::move(prediction));
    }
  }
  return result;
}

std::string MetaPatternAnalyzer::export_text(const MetaPatternReport& report) const {
  std::ostringstream out;
  out << "Pattern objects: " << report.objects.size() << "\n"
      << "Meta-patterns: " << report.meta_patterns.size() << "\n"
      << "Predictions: " << report.predictions.size() << "\n";
  for (const auto& meta : report.meta_patterns) {
    out << meta.id << " [" << meta.law << "] score=" << meta.family_score
        << " independent_realizations=" << meta.independent_realizations << "\n";
    for (const auto& reason : meta.reasons) out << "  reason: " << reason << "\n";
  }
  for (const auto& prediction : report.predictions) {
    out << prediction.id << " predicts " << prediction.predicted_role
        << " confidence=" << prediction.confidence << " justified=" << (prediction.justified ? "yes" : "no") << "\n";
  }
  return out.str();
}

std::string MetaPatternAnalyzer::export_json(const MetaPatternReport& report) const {
  std::ostringstream out;
  out << "{\"objects\":" << report.objects.size() << ",\"meta_patterns\":[";
  for (size_t i = 0; i < report.meta_patterns.size(); ++i) {
    if (i) out << ',';
    const auto& meta = report.meta_patterns[i];
    out << "{\"id\":\"" << meta.id << "\",\"law\":\"" << meta.law
        << "\",\"score\":" << meta.family_score << ",\"independent_realizations\":"
        << meta.independent_realizations << "}";
  }
  out << "],\"predictions\":" << report.predictions.size() << "}";
  return out.str();
}

const char* to_string(const MetaPattern&) { return "meta_pattern"; }

}  // namespace opforge::patterns
