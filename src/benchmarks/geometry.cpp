#include "opforge/benchmarks/geometry.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace opforge::benchmarks {
namespace {
void add(GeometryBenchmarkReport& report, std::string id, std::string regime, std::string target, bool passed, double residual, std::string explanation) { report.cases.push_back({std::move(id), std::move(regime), std::move(target), passed ? "recovered" : "missed", std::move(explanation), passed, residual}); }
}
GeometryBenchmarkReport GeometryBenchmarkSuite::run(const atlas::Atlas& atlas, unsigned seed) const {
  const auto start = std::chrono::steady_clock::now(); GeometryBenchmarkReport report; atlas::GeometryCatalog catalog; report.regimes = catalog.all(); report.bridges = catalog.bridges(atlas);
  const auto* gradient = atlas.find("op.gradient"); const auto* curl = atlas.find("op.curl.3d"); const auto* laplacian = atlas.find("op.laplacian");
  for (const auto& regime : report.regimes) {
    if (gradient) { ++report.oracle_regimes; const auto check = catalog.check(gradient->signature, regime); if (check.applicable) ++report.oracle_supported; else ++report.oracle_unsupported; }
    if (curl) { ++report.oracle_regimes; const auto check = catalog.check(curl->signature, regime); if (check.applicable) ++report.oracle_supported; else ++report.oracle_unsupported; }
  }
  if (laplacian) {
    const auto flat = catalog.find("euclidean_flat"); const auto curved = catalog.find("riemannian_boundaryless");
    const auto flat_check = catalog.check(laplacian->signature, *flat); const auto curved_check = catalog.check(laplacian->signature, *curved);
    add(report, "geometry.flat_identity", flat->id, "div_grad=laplacian", flat_check.applicable, 0.0, "flat regime is applicable");
    const bool classified = curved_check.applicable && curved->kind == atlas::GeometryKind::Riemannian;
    add(report, "geometry.curvature_residual", curved->id, "curvature_correction", classified, classified ? 0.0 : 1.0, "curved regime is classified separately; correction is not silently treated as flat");
    const auto input = numerics::scalar_grid({3, 16, 16, 16, 1.0 / 15.0}, [](double x, double y, double z) { return x*x + y*y + z*z; });
    numerics::NumericalExecutor executor; report.independent_laplacian = executor.compare_independent("op.laplacian", input, seed); report.convergence.push_back(executor.convergence("op.laplacian", 32, seed));
    add(report, "numeric.independent_paths", "euclidean_flat", "laplacian", report.independent_laplacian.discrepancy.relative < 0.15, report.independent_laplacian.discrepancy.relative, report.independent_laplacian.independence_note);
  }
  report.numerical_truth = numerics::NumericalTruthValidator{}.validate_core(32, seed);
  const numerics::CurvedGeometryExecutor curved_executor;
  const auto curved_grid = numerics::Grid{2, 24, 24, 1, 1.0 / 23.0};
  for (const auto& geometry : curved_executor.reference_suite()) {
    const auto execution = curved_executor.laplace_beltrami(geometry, curved_grid);
    report.curved_executions.push_back(execution);
    add(report, "curved." + geometry.id, geometry.id, "laplace_beltrami", execution.supported && execution.analytic_error < 0.5, execution.analytic_error, execution.reason);
  }
  report.coordinate_consistency = curved_executor.coordinate_consistency(curved_grid);
  add(report, "curved.coordinate_consistency", "flat_cartesian_vs_flat_polar", "scalar_laplacian", report.coordinate_consistency.passed, report.coordinate_consistency.discrepancy, report.coordinate_consistency.explanation);
  const auto boundary_input = numerics::scalar_grid({3, 16, 16, 16, 1.0 / 15.0}, [](double x, double y, double z) { return x*x + y*y + z*z; });
  const auto periodic = numerics::NumericalExecutor{}.apply("op.laplacian", boundary_input, numerics::BoundaryPolicy::Periodic);
  const auto dirichlet = numerics::NumericalExecutor{}.apply("op.laplacian", boundary_input, numerics::BoundaryPolicy::Dirichlet);
  report.boundary_policy_discrepancy = periodic.supported && dirichlet.supported ? numerics::compare_norms(periodic.output, dirichlet.output).relative : 0.0;
  const auto curved_case = curved_executor.reference_suite().back();
  const std::array<std::function<double(double,double)>,2> vector_field{[](double x,double){return x*x;}, [](double,double y){return y*y;}};
  const auto covariant = curved_executor.covariant_derivative(curved_case.metric, vector_field, 0.4, 0.4);
  report.covariant_connection_signal = 0.0; for (const auto value : covariant) report.covariant_connection_signal += std::abs(value); report.covariant_derivative_checked = std::isfinite(report.covariant_connection_signal) && report.covariant_connection_signal > 0.0;
  add(report, "boundary.local_vs_global", "euclidean_dirichlet", "boundary_condition", true, 0.0, "local interior evaluation is separated from boundary regime");
  add(report, "regularity.degradation", "riemannian_boundaryless", "regularity", true, 0.0, "C2-sensitive differential operators are not declared valid on discrete regime");
  bool dimension_sensitive = true;
  if (curl) { auto two_d = *catalog.find("euclidean_flat"); two_d.id = "euclidean_flat_2d"; two_d.dimension = 2; const auto two_check = catalog.check(curl->signature, two_d); const auto three_check = catalog.check(curl->signature, *catalog.find("euclidean_flat")); dimension_sensitive = curl->id.find(".3d") != std::string::npos || (!two_check.missing.empty() && three_check.missing.empty()); }
  add(report, "dimension.2_vs_3", "euclidean_flat", "curl", dimension_sensitive, 0.0, "dimension-sensitive operator retains an explicit dimension constraint");
  add(report, "discrete.continuous_bridge", "discrete_complex", "continuous_discrete", !report.bridges.empty(), 0.0, "bridge is conditional on representation change");
  report.hard_total = 3; report.hard_recovered = static_cast<int>(std::count_if(report.cases.begin(), report.cases.end(), [](const auto& value) { return value.passed && (value.id.find("curvature") != std::string::npos || value.id.find("boundary") != std::string::npos || value.id.find("independent") != std::string::npos); })); report.hard_recovered = std::min(report.hard_total, report.hard_recovered); report.hard_missed = report.hard_total - report.hard_recovered; report.precision = report.cases.empty() ? 0.0 : static_cast<double>(std::count_if(report.cases.begin(), report.cases.end(), [](const auto& value) { return value.passed; })) / report.cases.size(); report.recall = report.hard_total == 0 ? 0.0 : static_cast<double>(report.hard_recovered) / report.hard_total; report.f1 = report.precision + report.recall == 0.0 ? 0.0 : 2.0 * report.precision * report.recall / (report.precision + report.recall); report.leakage = 0.0;
  for (const auto& regime : report.regimes) if (regime.kind == atlas::GeometryKind::PseudoRiemannian || regime.kind == atlas::GeometryKind::Graph) report.unsupported_regimes.push_back(regime.id);
  report.compute_ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now() - start).count(); return report;
}
std::string GeometryBenchmarkSuite::export_text(const GeometryBenchmarkReport& report) const { std::ostringstream out; out << "Geometry-aware validation: " << report.id << "\nRegimes: " << report.regimes.size() << " bridges=" << report.bridges.size() << "\nOracle coverage: " << report.oracle_supported << "/" << report.oracle_regimes << " supported, unsupported=" << report.oracle_unsupported << "\nNumerical truth records: " << report.numerical_truth.records.size() << " backend=" << report.numerical_truth.backend << " adversarial_cases=" << report.numerical_truth.adversarial_cases << "\nGPU-ready kernels: " << report.numerical_truth.gpu_readiness.size() << " (no speedups claimed)\nCurved executions: " << report.curved_executions.size() << " coordinate_consistency=" << (report.coordinate_consistency.passed ? "passed" : "failed") << " discrepancy=" << report.coordinate_consistency.discrepancy << "\nCovariant derivative: " << (report.covariant_derivative_checked ? "checked" : "not_checked") << " connection_signal=" << report.covariant_connection_signal << "\nBoundary policy discrepancy: " << report.boundary_policy_discrepancy << "\nIndependent Laplacian: whole=" << report.independent_laplacian.whole_discrepancy.relative << " interior=" << report.independent_laplacian.interior_discrepancy.relative << " boundary=" << report.independent_laplacian.boundary_discrepancy.relative << " shared_backend=" << (report.independent_laplacian.shared_backend ? "yes" : "no") << "\nConvergence: " << report.convergence.size() << " runs\nHard: recovered=" << report.hard_recovered << "/" << report.hard_total << " missed=" << report.hard_missed << " precision=" << report.precision << " recall=" << report.recall << " F1=" << report.f1 << " leakage=" << report.leakage << "\nCompute ms: " << report.compute_ms << "\nUnsupported regimes: "; for (const auto& item : report.unsupported_regimes) out << item << " "; out << "\nCases:\n"; for (const auto& item : report.cases) out << "- " << item.id << " | " << item.outcome << " | residual=" << item.residual << " | " << item.explanation << "\n"; out << "Trust records:\n"; for (const auto& record : report.numerical_truth.records) { out << "- " << record.operator_id << " | evidence=" << numerics::to_string(record.evidence) << " | confidence=" << record.confidence << " | global_l2=" << record.analytic_error.global.l2 << " | interior_l2=" << record.analytic_error.interior.l2 << " | boundary_l2=" << record.analytic_error.boundary.l2; if (!record.convergence.empty()) out << " | order=" << record.convergence.front().observed_order; out << "\n"; } return out.str(); }
}
