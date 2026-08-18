#pragma once

#include "opforge/numerics/executor.hpp"
#include <string>
#include <vector>

namespace opforge::numerics {

enum class EvidenceLevel { SmokeTest, Convergent, IndependentPathAgreement, AnalyticReferenceMatch, GeometryValidated, BoundaryValidated, HighConfidenceNumerical };

struct RegionErrorSummary { NormSummary global, interior, boundary; };
struct ErrorBudget { double truncation{0}, boundary{0}, floating_point{0}, discretization_mismatch{0}, unsupported_geometry{0}; };
struct AnalyticCase { std::string id, operator_id, family, construction; NumericObject input, expected; };
struct NumericalTrustRecord {
  std::string operator_id, analytic_reference, confidence_explanation;
  EvidenceLevel evidence{EvidenceLevel::SmokeTest};
  double confidence{0};
  RegionErrorSummary analytic_error;
  std::vector<ConvergenceResult> convergence;
  IndependentExecution independent;
  ErrorBudget error_budget;
  std::vector<std::string> geometry_regimes, regularity_regimes;
  std::vector<std::string> tested_field_families;
  std::vector<int> adaptive_resolutions;
  bool boundary_validated{false}, geometry_validated{false};
};
struct GpuReadiness { std::string kernel, rationale; int estimated_parallel_cells{0}; };
struct NumericalTruthReport {
  std::vector<NumericalTrustRecord> records;
  std::vector<std::string> gpu_ready_kernels;
  std::vector<GpuReadiness> gpu_readiness;
  std::vector<std::string> regularity_stress_regimes;
  int adversarial_cases{0};
  std::string backend{"cpu_reference"};
  double compute_ms{0};
};

class AnalyticReferenceSuite {
public:
  AnalyticCase make(const std::string& operator_id, const Grid&) const;
  std::vector<std::string> supported_operators() const;
};

class NumericalTruthValidator {
public:
  NumericalTrustRecord validate(const std::string& operator_id, int max_resolution = 32, unsigned seed = 17) const;
  NumericalTruthReport validate_core(int max_resolution = 32, unsigned seed = 17) const;
};

RegionErrorSummary compare_regions(const NumericObject&, const NumericObject&);
const char* to_string(EvidenceLevel);

} // namespace opforge::numerics
