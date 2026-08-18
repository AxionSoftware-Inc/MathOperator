#pragma once

#include "opforge/atlas/geometry.hpp"
#include "opforge/numerics/curved.hpp"
#include "opforge/numerics/executor.hpp"
#include "opforge/numerics/truth.hpp"
#include <string>
#include <vector>

namespace opforge::benchmarks {
struct GeometryStressCase { std::string id, regime, target, outcome, explanation; bool passed{false}; double residual{0.0}; };
struct GeometryBenchmarkReport {
  std::string id{"geometry-aware-v0.7"};
  std::vector<atlas::GeometryRegime> regimes;
  std::vector<atlas::GeometryBridge> bridges;
  std::vector<GeometryStressCase> cases;
  std::vector<numerics::ConvergenceResult> convergence;
  numerics::IndependentExecution independent_laplacian;
  numerics::NumericalTruthReport numerical_truth;
  std::vector<numerics::CurvedExecutionResult> curved_executions;
  numerics::CoordinateConsistencyResult coordinate_consistency;
  double boundary_policy_discrepancy{0}, covariant_connection_signal{0};
  bool covariant_derivative_checked{false};
  int oracle_regimes{0}, oracle_supported{0}, oracle_unsupported{0};
  int hard_total{0}, hard_recovered{0}, hard_missed{0};
  double precision{0.0}, recall{0.0}, f1{0.0}, leakage{0.0}, compute_ms{0.0};
  std::vector<std::string> unsupported_regimes;
};
class GeometryBenchmarkSuite {
public:
  GeometryBenchmarkReport run(const atlas::Atlas&, unsigned seed = 17) const;
  std::string export_text(const GeometryBenchmarkReport&) const;
};
} // namespace opforge::benchmarks
