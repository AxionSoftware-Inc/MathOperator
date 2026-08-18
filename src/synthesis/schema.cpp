#include "opforge/synthesis/schema.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace opforge::synthesis {
namespace {

void unique(std::vector<std::string>& values, const std::string& value) {
  if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

std::string role_kind(const atlas::OperatorRecord* op) {
  if (!op) return "unknown";
  switch (op->signature.output_kind) {
    case atlas::ObjectKind::Scalar: return "scalar";
    case atlas::ObjectKind::Vector: return "vector";
    case atlas::ObjectKind::Tensor: return "tensor";
    case atlas::ObjectKind::DifferentialForm: return "differential_form";
    case atlas::ObjectKind::Matrix: return "matrix";
    case atlas::ObjectKind::Field: return "field";
    case atlas::ObjectKind::Unknown: return "unknown";
  }
  return "unknown";
}

ConstructionSchema schema_from_meta(const atlas::Atlas& atlas, const patterns::MetaPattern& meta) {
  ConstructionSchema schema;
  schema.id = "schema." + meta.id;
  schema.name = meta.law;
  schema.canonical_form = meta.canonical_law;
  schema.compression_gain = meta.member_pattern_ids.size() > 1
                                ? std::min(1.0, meta.member_pattern_ids.size() / 8.0)
                                : 0.0;
  schema.source_meta_patterns.push_back(meta.id);
  schema.participating_spaces = meta.participating_domains;
  schema.assumptions = meta.assumptions;
  schema.evidence = meta.reasons;
  if (meta.canonical_law == "next ∘ previous = 0") {
    schema.kind = SchemaKind::GradedFamily;
    schema.roles = {{"X0", "X0", "X1", "graded_object", "", -1, true},
                    {"X1", "X1", "X2", "graded_object", "", -1, true},
                    {"X2", "X2", "zero", "zero", "", -1, true}};
    schema.required_identities.push_back("next ∘ previous = 0");
    schema.structural_constraints.push_back("adjacent composition annihilates");
    schema.realization_rules.push_back({"B ∘ A = 0", "typed adjacent grades", "independent zero-composition patterns"});
  } else if (meta.canonical_law == "continuous_or_discrete_analogue") {
    schema.kind = SchemaKind::TransformRelation;
    schema.roles = {{"continuous", "continuous", "continuous", "operator", "", -1, true},
                    {"discrete", "discrete", "discrete", "operator", "", -1, true}};
    schema.structural_constraints.push_back("compatible discretization preserves role graph");
    schema.realization_rules.push_back({"D_h ≈ D", "compatible grid or complex", "continuous/discrete bridge relations"});
  } else if (meta.canonical_law == "operator_family") {
    schema.kind = SchemaKind::OperatorFamily;
    schema.roles.push_back({"O", "family.domain", "family.codomain", "operator", "", -1, true});
    schema.structural_constraints.push_back("shared typed signature properties");
    schema.realization_rules.push_back({"O_i : X_i → Y_i", "same abstract signature", "family pattern"});
  } else {
    schema.kind = SchemaKind::OperatorFamily;
    schema.roles = {{"A", "X", "Y", "operator", "", -1, true},
                    {"B", "Y", "Z", "operator", "", -1, true}};
    schema.structural_constraints.push_back("role graph is repeated across realizations");
    schema.realization_rules.push_back({"B ∘ A", "typed composition", "meta-pattern realizations"});
  }
  for (const auto& role : schema.roles) {
    if (const auto* op = atlas.find(role.input_space)) unique(schema.evidence, role_kind(op));
  }
  return schema;
}

}  // namespace

const char* to_string(SchemaKind kind) {
  switch (kind) {
    case SchemaKind::Operator: return "operator";
    case SchemaKind::OperatorFamily: return "operator_family";
    case SchemaKind::ParameterizedOperator: return "parameterized_operator";
    case SchemaKind::Decomposition: return "decomposition";
    case SchemaKind::Factorization: return "factorization";
    case SchemaKind::CorrectionLaw: return "correction_law";
    case SchemaKind::GradedFamily: return "graded_family";
    case SchemaKind::TransformRelation: return "transform_relation";
    case SchemaKind::ProjectionRecovery: return "projection_recovery";
  }
  return "unknown";
}

SchemaDiscoveryReport SchemaInducer::induce(const atlas::Atlas& atlas,
                                            const patterns::PatternReport& patterns,
                                            const patterns::MetaPatternReport& meta) const {
  SchemaDiscoveryReport report;
  for (const auto& meta_pattern : meta.meta_patterns) report.schemas.push_back(schema_from_meta(atlas, meta_pattern));

  std::map<std::string, std::vector<std::string>> factorization_realizations;
  for (const auto& identity : atlas.identities()) {
    if (!identity.left || !identity.right || identity.left->kind != atlas::Expression::Kind::Composition) continue;
    if (identity.right->kind != atlas::Expression::Kind::OperatorReference) continue;
    const auto key = "factorization:" + identity.right->value;
    factorization_realizations[key].push_back(identity.id);
  }
  for (const auto& [key, realizations] : factorization_realizations) {
    if (realizations.size() < 2) continue;
    ConstructionSchema schema;
    schema.id = "schema.factorization." + std::to_string(report.schemas.size() + 1);
    schema.name = "repeated factorization schema";
    schema.kind = SchemaKind::Factorization;
    schema.canonical_form = "O = B ∘ A";
    schema.roles = {{"A", "X", "Y", "operator", "", -1, true},
                    {"B", "Y", "Z", "operator", "", -1, true},
                    {"O", "X", "Z", "operator", "", -1, true}};
    schema.realizations = {};
    for (const auto& realization : realizations) schema.realizations.push_back({realization, "identity-backed", realization});
    schema.compression_gain = std::min(1.0, realizations.size() / 4.0);
    schema.evidence = realizations;
    schema.structural_constraints.push_back("composition and named operator have compatible typed signature");
    report.schemas.push_back(std::move(schema));
  }

  std::map<std::string, std::vector<const atlas::OperatorRecord*>> signature_families;
  for (const auto* op : atlas.all()) {
    const auto key = op->signature.domain.id + "->" + op->signature.codomain.id +
                     "|kind=" + std::to_string(static_cast<int>(op->signature.input_kind));
    signature_families[key].push_back(op);
  }
  for (const auto& [key, members] : signature_families) {
    if (members.size() < 2) continue;
    ParameterizedFamily family;
    family.id = "family.parameterized." + std::to_string(report.parameterized_families.size() + 1);
    family.schema_id = "schema.parameterized." + std::to_string(report.schemas.size() + 1);
    family.status = "inferred";
    for (size_t i = 0; i < members.size(); ++i) family.parameters.push_back("alpha_" + std::to_string(i));
    family.constraints.push_back("typed linear combination only");
    family.normalization.push_back("sum(alpha_i)=1 for recovery normalization");
    family.reduced_parameter_values.push_back("one-hot recovery points");
    for (const auto* member : members) {
      family.realizations.push_back(member->id);
      family.evidence.push_back(key);
    }
    report.parameterized_families.push_back(std::move(family));
    ConstructionSchema schema;
    schema.id = "schema.parameterized." + std::to_string(report.schemas.size() + 1);
    schema.name = "typed parameterized operator family";
    schema.kind = SchemaKind::ParameterizedOperator;
    schema.canonical_form = "O_alpha = sum(alpha_i O_i)";
    schema.free_parameters = report.parameterized_families.back().parameters;
    schema.structural_constraints = report.parameterized_families.back().constraints;
    schema.assumptions.push_back("all realizations share a typed linear signature");
    schema.realizations = {};
    for (const auto* member : members) schema.realizations.push_back({member->id, "shared signature", member->id});
    schema.compression_gain = std::min(1.0, members.size() / 4.0);
    report.schemas.push_back(std::move(schema));
  }

  for (const auto& schema : report.schemas) {
    if (schema.compression_gain >= 0.5 && schema.source_meta_patterns.size() + schema.realizations.size() >= 2)
      report.discovery_leads.push_back(schema);
  }

  report.completions = complete(atlas, report);
  for (const auto& schema : report.schemas) {
    if (schema.kind == SchemaKind::Factorization) {
      LawCandidate law;
      law.id = "law." + schema.id;
      law.law_type = "factorization";
      law.lhs = "O";
      law.rhs = "B ∘ A";
      law.source_schema = schema.id;
      law.status = "structurally_supported";
      law.evidence = schema.evidence;
      report.laws.push_back(std::move(law));
    }
  }
  (void)patterns;
  return report;
}

std::vector<SchemaCompletion> SchemaInducer::complete(const atlas::Atlas& atlas,
                                                       const SchemaDiscoveryReport& report) const {
  std::vector<SchemaCompletion> completions;
  int index = 1;
  for (const auto& schema : report.schemas) {
    if (schema.kind != SchemaKind::GradedFamily && schema.kind != SchemaKind::Factorization) continue;
    SchemaCompletion completion;
    completion.id = "SC-" + std::to_string(index++);
    completion.schema_id = schema.id;
    completion.missing_role = schema.kind == SchemaKind::GradedFamily ? "next" : "factor operator O";
    completion.expected_object_kind = schema.kind == SchemaKind::GradedFamily ? "graded_operator" : "operator";
    completion.expected_symmetry = "preserve typed role graph";
    completion.expected_order = schema.kind == SchemaKind::GradedFamily ? 1 : -1;
    completion.expected_spaces = schema.participating_spaces;
    completion.assumptions = schema.assumptions;
    completion.required_identities = schema.required_identities;
    completion.reasons = {"schema has multiple realizations", "missing role is constrained by the common role graph", "ordinary graph gap alone is insufficient"};
    completion.confidence = std::min(0.95, 0.5 + schema.compression_gain * 0.4);
    completion.justified = schema.compression_gain >= 0.25;
    if (completion.justified) completions.push_back(std::move(completion));
  }
  (void)atlas;
  return completions;
}

std::string SchemaInducer::export_text(const SchemaDiscoveryReport& report) const {
  std::ostringstream out;
  out << "Schemas: " << report.schemas.size() << "\n"
      << "Parameterized families: " << report.parameterized_families.size() << "\n"
      << "Completions: " << report.completions.size() << "\n"
      << "Laws: " << report.laws.size() << "\n"
      << "Discovery leads: " << report.discovery_leads.size() << "\n";
  for (const auto& schema : report.schemas)
    out << schema.id << " [" << to_string(schema.kind) << "] " << schema.canonical_form
        << " compression=" << schema.compression_gain << " status=" << schema.status << "\n";
  return out.str();
}

std::string SchemaInducer::export_json(const SchemaDiscoveryReport& report) const {
  std::ostringstream out;
  out << "{\"schemas\":" << report.schemas.size()
      << ",\"parameterized_families\":" << report.parameterized_families.size()
      << ",\"completions\":" << report.completions.size()
      << ",\"laws\":" << report.laws.size()
      << ",\"discovery_leads\":" << report.discovery_leads.size() << "}";
  return out.str();
}

}  // namespace opforge::synthesis
