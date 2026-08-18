#pragma once
#include "opforge/atlas/model.hpp"
#include <string>
#include <functional>
#include <vector>

namespace opforge::numerics {
struct Grid { int dimension{3}, nx{1}, ny{1}, nz{1}; double step{1.0}; };
struct NumericObject { enum class Kind { Scalar, Vector, Matrix }; Kind kind{Kind::Scalar}; Grid grid; int components{1}; std::vector<double> values; };
enum class ExecutionBackend { CPU, GPUOptional };
enum class BoundaryPolicy { OneSided, Periodic, Dirichlet, Neumann };
struct ExecutionResult { bool supported{false}; NumericObject output; std::string reason, backend{"finite_difference"}; double runtime_ms{0}, max_error{0}; unsigned seed{0}; ExecutionBackend execution_backend{ExecutionBackend::CPU}; };
enum class NormKind { Max, L2, Relative };
struct NormSummary { double max{0}, l2{0}, relative{0}; NormKind primary{NormKind::L2}; };
struct IndependentExecution { ExecutionResult direct, composed; NormSummary discrepancy, whole_discrepancy, interior_discrepancy, boundary_discrepancy; bool shared_backend{false}; std::string route, independence_note; };
struct ConvergenceResult { std::string operator_id; std::vector<int> resolutions; std::vector<double> errors; std::vector<double> max_errors; std::vector<double> interior_errors, boundary_errors; std::vector<NormSummary> norm_history; double observed_order{0}; bool passed{false}; bool stable{false}; double boundary_error{0}, interior_error{0}; std::string backend{"cpu"}; };
NumericObject scalar_grid(const Grid&, const std::function<double(double,double,double)>&);
class NumericalExecutor {
public:
  ExecutionResult apply(const std::string& operator_id, const NumericObject& input, unsigned seed=0) const;
  ExecutionResult apply(const std::string& operator_id, const NumericObject& input, BoundaryPolicy policy, unsigned seed=0) const;
  ExecutionResult apply(const atlas::ExpressionPtr&, const NumericObject& input, const atlas::Atlas&, unsigned seed=0) const;
  ExecutionResult apply_direct(const std::string& operator_id, const NumericObject& input, unsigned seed=0) const;
  IndependentExecution compare_independent(const std::string& operator_id, const NumericObject& input, unsigned seed=0) const;
  ConvergenceResult convergence(const std::string& operator_id, int max_resolution=32, unsigned seed=0) const;
};
double l2_error(const NumericObject&, const NumericObject&);
NormSummary compare_norms(const NumericObject&, const NumericObject&);
std::vector<ConvergenceResult> validate_seed_backend();
}
