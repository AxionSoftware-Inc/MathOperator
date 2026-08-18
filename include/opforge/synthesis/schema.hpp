#pragma once

#include "opforge/patterns/meta.hpp"
#include "opforge/research/residual.hpp"
#include "opforge/synthesis/candidate.hpp"

#include <string>
#include <vector>

namespace opforge::synthesis {

enum class SchemaKind { Operator, OperatorFamily, ParameterizedOperator, Decomposition,
                        Factorization, CorrectionLaw, GradedFamily, TransformRelation,
                        ProjectionRecovery };

struct TypedRole {
  std::string id, input_space, output_space, object_kind, symmetry;
  int differential_order{-1};
  bool required{true};
};

struct RealizationRule { std::string expression, condition, evidence; };

struct ConstructionSchema {
  std::string id, name, canonical_form, status{"inferred"};
  SchemaKind kind{SchemaKind::OperatorFamily};
  std::vector<TypedRole> roles;
  std::vector<std::string> participating_spaces, structural_constraints, free_parameters;
  std::vector<std::string> required_identities, assumptions, evidence, source_meta_patterns;
  std::vector<RealizationRule> realization_rules, realizations;
  double compression_gain{0.0};
};

struct ParameterizedFamily {
  std::string id, schema_id, status{"inferred"};
  std::vector<std::string> parameters, constraints, normalization, realizations, evidence;
  std::vector<std::string> reduced_parameter_values;
};

struct SchemaCompletion {
  std::string id, schema_id, missing_role, expected_object_kind, expected_symmetry;
  int expected_order{-1};
  std::vector<std::string> expected_spaces, assumptions, required_identities, reasons;
  double confidence{0.0};
  bool justified{false};
};

struct LawCandidate {
  std::string id, law_type, lhs, rhs, status{"inferred"}, source_schema;
  std::vector<std::string> assumptions, evidence, residuals;
  bool trivial{false}, equivalent_to_known{false};
};

struct SchemaDiscoveryReport {
  std::vector<ConstructionSchema> schemas;
  std::vector<ParameterizedFamily> parameterized_families;
  std::vector<SchemaCompletion> completions;
  std::vector<LawCandidate> laws;
  std::vector<ConstructionSchema> discovery_leads;
};

class SchemaInducer {
public:
  SchemaDiscoveryReport induce(const atlas::Atlas&, const patterns::PatternReport&,
                               const patterns::MetaPatternReport&) const;
  std::vector<SchemaCompletion> complete(const atlas::Atlas&, const SchemaDiscoveryReport&) const;
  std::string export_text(const SchemaDiscoveryReport&) const;
  std::string export_json(const SchemaDiscoveryReport&) const;
};

const char* to_string(SchemaKind);

}  // namespace opforge::synthesis
