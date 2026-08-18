#include "opforge/atlas/loader.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <tuple>

namespace opforge::atlas {
namespace {

std::string field(const std::string& object, const std::string& name) {
  const std::regex pattern("\\\"" + name + "\\\":\\\"([^\\\"]*)\\\"");
  std::smatch match;
  return std::regex_search(object, match, pattern) ? match[1].str() : std::string{};
}

bool boolean_field(const std::string& object, const std::string& name, bool fallback = false) {
  const std::regex pattern("\\\"" + name + "\\\":(true|false)");
  std::smatch match;
  if (!std::regex_search(object, match, pattern)) return fallback;
  return match[1].str() == "true";
}

std::vector<std::string> string_array(const std::string& object, const std::string& name) {
  const std::regex pattern("\\\"" + name + "\\\":\\[([^\\]]*)\\]");
  std::smatch match;
  if (!std::regex_search(object, match, pattern)) return {};

  std::vector<std::string> values;
  const std::regex item("\\\"([^\\\"]+)\\\"");
  for (std::sregex_iterator it(match[1].first, match[1].second, item), end; it != end; ++it) {
    values.push_back((*it)[1].str());
  }
  return values;
}

std::string expression_key(const ExpressionPtr& expression) {
  if (!expression) return "<null>";
  std::string result = std::to_string(static_cast<int>(expression->kind)) + ":" + expression->value + "(";
  for (const auto& child : expression->children) result += expression_key(child) + ",";
  return result + ")";
}

ObjectKind infer_kind(const std::string& space) {
  if (space.rfind("scalar", 0) == 0 || space.rfind("function", 0) == 0 || space.rfind("frequency", 0) == 0)
    return ObjectKind::Scalar;
  if (space.rfind("vector", 0) == 0 || space.rfind("grid.vector", 0) == 0) return ObjectKind::Vector;
  if (space.rfind("matrix", 0) == 0 || space.rfind("spectral", 0) == 0) return ObjectKind::Matrix;
  if (space.rfind("form", 0) == 0) return ObjectKind::DifferentialForm;
  if (space.rfind("tensor", 0) == 0) return ObjectKind::Tensor;
  return ObjectKind::Field;
}

RelationKind relation_kind(const std::string& value) {
  if (value == "maps_to") return RelationKind::MapsTo;
  if (value == "composes_after") return RelationKind::ComposesAfter;
  if (value == "equal_to") return RelationKind::EqualTo;
  if (value == "composition") return RelationKind::Composition;
  if (value == "analog_of" || value == "generalizes") return RelationKind::Generalizes;
  if (value == "special_case_of") return RelationKind::SpecialCaseOf;
  if (value == "dual") return RelationKind::Dual;
  if (value == "preserves") return RelationKind::Preserves;
  if (value == "discretized_by") return RelationKind::DiscretizedBy;
  if (value == "continuous_analog") return RelationKind::ContinuousAnalog;
  if (value == "discrete_analog") return RelationKind::DiscreteAnalog;
  if (value == "adjoint_of") return RelationKind::AdjointOf;
  if (value == "inverse_of") return RelationKind::InverseOf;
  if (value == "factorization") return RelationKind::Factorization;
  if (value == "decomposition") return RelationKind::Decomposition;
  if (value == "projection") return RelationKind::Projection;
  if (value == "inclusion") return RelationKind::Inclusion;
  if (value == "transform_correspondence") return RelationKind::TransformCorrespondence;
  if (value == "commutes_with") return RelationKind::CommutesWith;
  if (value == "anti_commutes_with") return RelationKind::AntiCommutesWith;
  if (value == "annihilates") return RelationKind::Annihilates;
  if (value == "conjugate_under") return RelationKind::ConjugateUnder;
  if (value == "requires_structure") return RelationKind::RequiresStructure;
  if (value == "preserves_invariant") return RelationKind::PreservesInvariant;
  if (value == "left_inverse") return RelationKind::LeftInverse;
  if (value == "right_inverse") return RelationKind::RightInverse;
  if (value == "component_of") return RelationKind::ComponentOf;
  if (value == "projection_of") return RelationKind::ProjectionOf;
  if (value == "inclusion_into") return RelationKind::InclusionInto;
  if (value == "restricts_to") return RelationKind::RestrictsTo;
  if (value == "extends") return RelationKind::Extends;
  if (value == "realization_of") return RelationKind::RealizationOf;
  if (value == "analogue_of") return RelationKind::AnalogueOf;
  if (value == "continuous_limit_of") return RelationKind::ContinuousLimitOf;
  if (value == "intertwines") return RelationKind::Intertwines;
  if (value == "closure_member") return RelationKind::ClosureMember;
  if (value == "generated_by") return RelationKind::GeneratedBy;
  return RelationKind::RelatedTo;
}

struct PendingRelation {
  std::string source, kind, target, condition, provenance;
};

struct PendingIdentity {
  std::string id, name, left, right, provenance, canonical_form;
  std::vector<std::string> assumptions, dimension_constraints, regularity_constraints,
      required_structures, applicable_domains;
};

struct PendingIdentityMetadata {
  std::string id, metric, orientation, boundary, scalar_field, object_grade, canonical_form, composition_outer, composition_inner, right_reference;
  std::vector<std::string> assumptions, dimension_constraints, regularity_constraints, applicable_domains;
};

}  // namespace

Atlas AtlasLoader::load_excluding(const std::string& path, const std::set<std::string>& excluded_files) {
  Atlas atlas;
  std::vector<std::filesystem::path> files;
  const std::filesystem::path root(path);
  if (std::filesystem::is_regular_file(root)) {
    files.push_back(root);
  } else if (std::filesystem::is_directory(root)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.path().extension() == ".json" && !excluded_files.contains(entry.path().filename().string())) files.push_back(entry.path());
    }
  }

  std::regex space(R"REGEX(\{"id":"([^"]+)","name":"([^"]+)","dimension":(-?[0-9]+)[^}]*\})REGEX");
  std::regex oper(R"REGEX(\{"id":"([^"]+)","name":"([^"]+)","domain":"([^"]+)","codomain":"([^"]+)","order":(-?[0-9]+)[^}]*\})REGEX");
  std::regex relation(R"REGEX(\{"from":"([^"]+)","kind":"([^"]+)","to":"([^"]+)"[^}]*\})REGEX");
  std::regex identity(R"REGEX(\{"id":"([^"]+)","name":"([^"]+)","left":"([^"]+)","right":"([^"]+)"[^}]*\})REGEX");
  std::regex identity_metadata(R"REGEX(\{"id":"([^"]+)"[^}]*\})REGEX");
  std::vector<PendingRelation> pending_relations;
  std::vector<PendingIdentity> pending_identities;
  std::vector<PendingIdentityMetadata> pending_metadata;

  for (const auto& file : files) {
    std::ifstream input(file);
    const std::string text((std::istreambuf_iterator<char>(input)), {});

    for (std::sregex_iterator it(text.begin(), text.end(), space), end; it != end; ++it) {
      atlas.add_space({(*it)[1], (*it)[2], "", "", std::stoi((*it)[3]), -1,
                       ScalarField::Real, false, false, false, true, false});
    }

    for (std::sregex_iterator it(text.begin(), text.end(), oper), end; it != end; ++it) {
      const std::string object = (*it)[0].str();
      OperatorRecord record;
      record.id = (*it)[1];
      record.name = (*it)[2];
      record.signature.domain = {(*it)[3], ""};
      record.signature.codomain = {(*it)[4], ""};
      record.signature.input_kind = infer_kind(record.signature.domain.id);
      record.signature.output_kind = infer_kind(record.signature.codomain.id);
      record.signature.differential_order = std::stoi((*it)[5]);
      record.signature.required_structures = string_array(object, "required_structures");
      record.signature.continuous = boolean_field(object, "continuous", true);
      record.signature.discrete = boolean_field(object, "discrete", false);
      record.signature.linear = boolean_field(object, "linear", true);
      record.signature.local = boolean_field(object, "local", true);
      record.definition = record.id.find(".zero") != std::string::npos
                              ? Expression::zero()
                              : Expression::ref(record.id);
      record.mathematical_domain = file.stem().string();
      record.provenance_category = field(object, "provenance");
      if (record.provenance_category.empty()) record.provenance_category = "imported_source";
      const bool numerical_default = record.mathematical_domain == "discrete" ||
                                     record.mathematical_domain == "vector_calculus" ||
                                     record.mathematical_domain == "linear_algebra";
      record.numerical_supported = boolean_field(object, "numerical_supported", numerical_default);
      const auto evidence_type = record.provenance_category == "imported_source" ? "source_verified" : record.provenance_category;
      record.evidence.push_back({record.id + ".import", evidence_type, file.string(), "0.25",
                                 "2026-08-15", record.id, "imported", file.string(), -1});
      atlas.add(std::move(record));
    }

    for (std::sregex_iterator it(text.begin(), text.end(), relation), end; it != end; ++it) {
      const std::string object = (*it)[0].str();
      pending_relations.push_back({(*it)[1], (*it)[2], (*it)[3], field(object, "condition"),
                                   field(object, "provenance")});
    }

    for (std::sregex_iterator it(text.begin(), text.end(), identity), end; it != end; ++it) {
      const std::string object = (*it)[0].str();
      PendingIdentity item;
      item.id = (*it)[1];
      item.name = (*it)[2];
      item.left = (*it)[3];
      item.right = (*it)[4];
      item.assumptions = string_array(object, "assumptions");
      item.dimension_constraints = string_array(object, "dimension_constraints");
      item.regularity_constraints = string_array(object, "regularity_constraints");
      item.required_structures = string_array(object, "required_structures");
      item.applicable_domains = string_array(object, "applicable_domains");
      item.provenance = field(object, "provenance");
      if (item.provenance.empty()) item.provenance = "imported_identity";
      item.canonical_form = field(object, "canonical_form");
      if (item.canonical_form.empty()) item.canonical_form = item.left + " = " + item.right;
      pending_identities.push_back(std::move(item));
    }
    if (file.stem() == "identity_assumptions" || file.stem() == "identities_v012" || file.stem() == "semantic_densification_v013") {
      for (std::sregex_iterator it(text.begin(), text.end(), identity_metadata), end; it != end; ++it) {
        const std::string object = (*it)[0].str();
        PendingIdentityMetadata metadata;
        metadata.id = (*it)[1];
        metadata.composition_outer = field(object, "composition_outer");
        metadata.composition_inner = field(object, "composition_inner");
        metadata.right_reference = field(object, "right_reference");
        metadata.assumptions = string_array(object, "assumptions");
        metadata.dimension_constraints = string_array(object, "dimension_constraints");
        metadata.regularity_constraints = string_array(object, "regularity_constraints");
        metadata.applicable_domains = string_array(object, "applicable_domains");
        metadata.metric = field(object, "metric");
        metadata.orientation = field(object, "orientation");
        metadata.boundary = field(object, "boundary");
        metadata.scalar_field = field(object, "scalar_field");
        metadata.object_grade = field(object, "object_grade");
        metadata.canonical_form = field(object, "canonical_form");
        pending_metadata.push_back(std::move(metadata));
      }
    }
  }

  for (const auto& relation : pending_relations) {
    const auto provenance = relation.provenance.empty() ? "imported_relation" : relation.provenance;
    atlas.add_relation(relation.source, {relation_kind(relation.kind), relation.target,
                                         relation.condition, provenance});
  }
  for (const auto& pending : pending_identities) {
    Identity identity;
    identity.id = pending.id;
    identity.name = pending.name;
    identity.left = Expression::ref(pending.left);
    identity.right = Expression::ref(pending.right);
    identity.assumptions = pending.assumptions;
    identity.dimension_constraints = pending.dimension_constraints;
    identity.regularity_constraints = pending.regularity_constraints;
    identity.required_structures = pending.required_structures;
    identity.applicable_domains = pending.applicable_domains;
    identity.canonical_form = pending.canonical_form;
    identity.provenance_category = pending.provenance;
    if (const auto metadata = std::find_if(pending_metadata.begin(), pending_metadata.end(),
                                           [&](const auto& item) { return item.id == pending.id; });
        metadata != pending_metadata.end()) {
      identity.assumptions = metadata->assumptions;
      identity.dimension_constraints = metadata->dimension_constraints;
      identity.regularity_constraints = metadata->regularity_constraints;
      identity.applicable_domains = metadata->applicable_domains;
      identity.metric = metadata->metric;
      identity.orientation = metadata->orientation;
      identity.boundary = metadata->boundary;
      identity.scalar_field = metadata->scalar_field;
      identity.object_grade = metadata->object_grade;
      identity.canonical_form = metadata->canonical_form;
      if (!metadata->composition_outer.empty() && !metadata->composition_inner.empty()) {
        identity.left = Expression::composition(Expression::ref(metadata->composition_outer),
                                                 Expression::ref(metadata->composition_inner));
      }
      if (!metadata->right_reference.empty()) identity.right = Expression::ref(metadata->right_reference);
    }
    if (const auto* left = atlas.find(pending.left)) {
      if (identity.applicable_domains.empty()) identity.applicable_domains.push_back(left->signature.domain.id);
      if (identity.dimension_constraints.empty()) {
        if (const auto* space = atlas.find_space(left->signature.domain.id); space && space->dimension >= 0)
          identity.dimension_constraints.push_back("dimension=" + std::to_string(space->dimension));
      }
      for (const auto& structure : left->signature.required_structures)
        if (std::find(identity.required_structures.begin(), identity.required_structures.end(), structure) == identity.required_structures.end())
          identity.required_structures.push_back(structure);
    }
    if (const auto* right = atlas.find(pending.right)) {
      for (const auto& structure : right->signature.required_structures)
        if (std::find(identity.required_structures.begin(), identity.required_structures.end(), structure) == identity.required_structures.end())
          identity.required_structures.push_back(structure);
    }
    identity.evidence.push_back({pending.id + ".import", pending.provenance, "atlas_loader", "0.25",
                                 "2026-08-15", pending.id, "imported", "", -1});
    const auto identity_key = expression_key(identity.left) + "=" + expression_key(identity.right);
    const bool duplicate_semantic = std::any_of(atlas.identities().begin(), atlas.identities().end(), [&](const auto& existing) {
      return expression_key(existing.left) + "=" + expression_key(existing.right) == identity_key;
    });
    if (!duplicate_semantic) atlas.add_identity(std::move(identity));
  }
  return atlas;
}

Atlas AtlasLoader::load(const std::string& path) { return load_excluding(path, {}); }

AtlasStats AtlasLoader::stats(const Atlas& atlas) {
  AtlasStats stats;
  stats.operators = atlas.all().size();
  stats.spaces = atlas.spaces().size();
  stats.identities = atlas.identities().size();
  for (const auto* operator_record : atlas.all()) {
    stats.relations += operator_record->relations.size();
    ++stats.operators_by_domain[operator_record->mathematical_domain];
    ++stats.provenance_breakdown[operator_record->provenance_category];
    if (operator_record->verification == VerificationStatus::Proposed || operator_record->evidence.empty())
      ++stats.unverified_facts;
    else
      ++stats.verified_facts;
    if (!operator_record->numerical_supported) ++stats.unsupported_numerical;
    if (operator_record->relations.empty()) ++stats.disconnected;
    for (const auto& relation : operator_record->relations) ++stats.relation_provenance_breakdown[relation.evidence];
  }
  for (const auto& identity : atlas.identities()) ++stats.identity_provenance_breakdown[identity.provenance_category];
  return stats;
}

std::vector<AtlasIssue> AtlasLoader::validate(const Atlas& atlas) { return atlas.validate(); }

AtlasAuditReport AtlasLoader::audit_v2(const Atlas& atlas) {
  AtlasAuditReport report;
  report.issues = atlas.validate();

  for (const auto* source : atlas.all()) {
    std::set<std::string> relation_keys;
    for (const auto& relation : source->relations) {
      const auto key = std::string(to_string(relation.kind)) + "|" + relation.target_id;
      if (!relation_keys.insert(key).second) {
        ++report.duplicate_relations;
        report.issues.push_back({"duplicate_semantic_relation", source->id + " -> " + relation.target_id});
      }
      const auto* target = atlas.find(relation.target_id);
      if (!target) continue;
      const bool source_continuous = source->signature.continuous;
      const bool target_continuous = target->signature.continuous;
      const bool source_discrete = source->signature.discrete;
      const bool target_discrete = target->signature.discrete;
      const bool opposite_continuous_discrete =
          (source_continuous && target_discrete && !source_discrete && !target_continuous) ||
          (source_discrete && target_continuous && !source_continuous && !target_discrete);
      if (relation.kind == RelationKind::DiscreteAnalog && !opposite_continuous_discrete) {
        ++report.bridge_type_mismatches;
        report.issues.push_back({"continuous_discrete_bridge_mismatch", source->id + " -> " + target->id});
      }
      const bool both_continuous = source_continuous && target_continuous && !source_discrete && !target_discrete;
      if (relation.kind == RelationKind::ContinuousAnalog && !(both_continuous || opposite_continuous_discrete)) {
        ++report.bridge_type_mismatches;
        report.issues.push_back({"continuous_bridge_mismatch", source->id + " -> " + target->id});
      }
    }
  }

  std::map<std::string, std::string> identity_pairs;
  for (const auto& identity : atlas.identities()) {
    if (identity.assumptions.empty() && identity.required_structures.empty() &&
        identity.dimension_constraints.empty() && identity.regularity_constraints.empty() &&
        identity.metric.empty() && identity.orientation.empty() && identity.boundary.empty() &&
        identity.scalar_field.empty() && identity.object_grade.empty()) {
      ++report.missing_identity_assumptions;
      report.missing_identity_assumption_ids.push_back(identity.id);
    }
    const auto pair = expression_key(identity.left) + "=" + expression_key(identity.right);
    if (const auto it = identity_pairs.find(pair); it != identity_pairs.end() && it->second != identity.canonical_form) {
      ++report.contradictory_identities;
      report.issues.push_back({"contradictory_identity", identity.id + " conflicts with another identity"});
    } else {
      identity_pairs[pair] = identity.canonical_form;
    }
  }
  return report;
}

AtlasAuditReport AtlasLoader::audit_v3(const Atlas& atlas) {
  AtlasAuditReport report = audit_v2(atlas);
  for (const auto* op : atlas.all()) {
    if (!atlas.find_space(op->signature.domain.id) || !atlas.find_space(op->signature.codomain.id)) {
      ++report.invalid_space_operator_compatibility;
      report.issues.push_back({"invalid_space_operator_compatibility", op->id});
    }
    if (op->signature.variance != "scalar" && op->signature.variance != "covariant" &&
        op->signature.variance != "contravariant" && op->signature.variance != "mixed") {
      ++report.impossible_variance;
      report.issues.push_back({"impossible_variance", op->id + " has unsupported variance " + op->signature.variance});
    }
    for (const auto& relation : op->relations) {
      if (relation.kind == RelationKind::AdjointOf && !atlas.find(relation.target_id)) {
        ++report.inconsistent_adjoint_pairs;
        report.issues.push_back({"inconsistent_adjoint_pair", op->id + " -> " + relation.target_id});
      }
    }
  }
  std::set<std::string> semantic_facts;
  for (const auto& identity : atlas.identities()) {
    const auto key = expression_key(identity.left) + "=" + expression_key(identity.right);
    if (!semantic_facts.insert(key).second) {
      ++report.duplicate_semantic_facts;
      report.issues.push_back({"duplicate_semantic_fact", identity.id});
    }
  }
  return report;
}

AtlasLoader::DiversityReport AtlasLoader::diversity(const Atlas& atlas) {
  DiversityReport report;
  std::set<std::string> domains, structures, relation_kinds, assumptions;
  std::map<std::string, std::set<std::string>> identity_domains;
  auto first_reference = [](const ExpressionPtr& expression, const auto& self) -> std::string {
    if (!expression) return {};
    if (expression->kind == Expression::Kind::OperatorReference) return expression->value;
    for (const auto& child : expression->children) if (const auto value = self(child, self); !value.empty()) return value;
    return {};
  };
  for (const auto* op : atlas.all()) {
    domains.insert(op->mathematical_domain); ++report.operators_per_domain[op->mathematical_domain];
    if (op->signature.continuous && !op->signature.discrete) ++report.continuous_operators;
    if (op->signature.discrete && !op->signature.continuous) ++report.discrete_operators;
    if (op->relations.empty()) report.isolated_operators.push_back(op->id);
    for (const auto& structure : op->signature.required_structures) { assumptions.insert(structure); structures.insert(structure); }
    for (const auto& relation : op->relations) {
      relation_kinds.insert(to_string(relation.kind)); ++report.relations_per_domain[op->mathematical_domain];
      const auto* target = atlas.find(relation.target_id); if (!target) continue;
      const bool bridge = relation.kind == RelationKind::ContinuousAnalog || relation.kind == RelationKind::DiscreteAnalog || relation.kind == RelationKind::Generalizes || relation.kind == RelationKind::SpecialCaseOf || relation.kind == RelationKind::Dual || relation.kind == RelationKind::RelatedTo;
      if (bridge) { ++report.bridges_per_domain_pair[op->mathematical_domain + " -> " + target->mathematical_domain]; if (op->signature.continuous != target->signature.continuous || op->signature.discrete != target->signature.discrete) ++report.continuous_discrete_bridges; }
    }
  }
  for (const auto& identity : atlas.identities()) {
    const auto id = first_reference(identity.left, first_reference); const auto* op = atlas.find(id);
    if (op) ++report.identities_per_domain[op->mathematical_domain];
  }
  report.domains.assign(domains.begin(), domains.end()); report.structure_kinds.assign(structures.begin(), structures.end()); report.relation_kinds.assign(relation_kinds.begin(), relation_kinds.end()); report.assumption_regimes.assign(assumptions.begin(), assumptions.end());
  for (const auto& domain : report.domains) {
    report.algebraic_coverage |= domain.find("algebra") != std::string::npos || domain.find("linear") != std::string::npos || domain.find("operator") != std::string::npos;
    report.geometric_coverage |= domain.find("geometry") != std::string::npos || domain.find("vector") != std::string::npos || domain.find("differential") != std::string::npos;
    report.spectral_coverage |= domain.find("spectral") != std::string::npos;
    report.variational_coverage |= domain.find("variational") != std::string::npos;
    report.discrete_coverage |= domain.find("discrete") != std::string::npos || domain.find("advanced") != std::string::npos;
  }
  report.independent_realizations = report.domains.size() > 1 ? report.domains.size() : 0;
  return report;
}

}  // namespace opforge::atlas
