#pragma once

#include "opforge/numerics/executor.hpp"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace opforge::numerics {

struct CoordinateChart2D { std::string id; std::array<double,2> origin{0.0,0.0}; std::array<std::function<double(double,double)>,2> embedding; };
struct Metric2D { std::string id; std::array<std::function<double(double,double)>,4> components; bool positive_definite{true}; };
struct CurvedGeometryCase { std::string id, description; CoordinateChart2D chart; Metric2D metric; std::function<double(double,double)> scalar; std::function<double(double,double)> analytic_laplacian; bool curvature_nonzero{false}; };
struct CurvedExecutionResult { bool supported{false}; NumericObject output; std::string geometry_id, reason; double min_determinant{0}, max_christoffel{0}, residual{0}, analytic_error{0}; };
struct CoordinateConsistencyResult { bool passed{false}; std::string first_geometry, second_geometry, explanation; double first_error{0}, second_error{0}, discrepancy{0}; };

class CurvedGeometryExecutor {
public:
  double determinant(const Metric2D&, double x, double y) const;
  std::array<double,4> inverse_metric(const Metric2D&, double x, double y) const;
  std::array<double,8> christoffel(const Metric2D&, double x, double y, double step=1e-5) const;
  CurvedExecutionResult laplace_beltrami(const CurvedGeometryCase&, const Grid&) const;
  std::array<double,4> covariant_derivative(const Metric2D&, const std::array<std::function<double(double,double)>,2>&, double x, double y, double step=1e-5) const;
  std::vector<CurvedGeometryCase> reference_suite() const;
  CoordinateConsistencyResult coordinate_consistency(const Grid&) const;
};

} // namespace opforge::numerics
