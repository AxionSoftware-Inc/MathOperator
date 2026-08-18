#include "opforge/numerics/curved.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opforge::numerics {
namespace {
int at(const Grid& g, int x, int y, int z, int c, int components) {
  x = std::clamp(x, 0, g.nx - 1); y = std::clamp(y, 0, g.ny - 1); z = std::clamp(z, 0, g.nz - 1);
  return (((z * g.ny + y) * g.nx + x) * components) + c;
}
double derivative(const std::function<double(double,double)>& f, double x, double y, int axis, double h) {
  return axis == 0 ? (f(x + h, y) - f(x - h, y)) / (2.0 * h) : (f(x, y + h) - f(x, y - h)) / (2.0 * h);
}
}

double CurvedGeometryExecutor::determinant(const Metric2D& m, double x, double y) const {
  return m.components[0](x,y) * m.components[3](x,y) - m.components[1](x,y) * m.components[2](x,y);
}

std::array<double,4> CurvedGeometryExecutor::inverse_metric(const Metric2D& m, double x, double y) const {
  const double d = determinant(m, x, y);
  if (std::abs(d) < 1e-14) return {NAN,NAN,NAN,NAN};
  return {m.components[3](x,y)/d, -m.components[1](x,y)/d, -m.components[2](x,y)/d, m.components[0](x,y)/d};
}

std::array<double,8> CurvedGeometryExecutor::christoffel(const Metric2D& m, double x, double y, double h) const {
  std::array<double,8> result{};
  const auto inverse = inverse_metric(m, x, y);
  for (int k=0;k<2;++k) for (int i=0;i<2;++i) for (int j=0;j<2;++j) {
    double value = 0.0;
    for (int l=0;l<2;++l) {
      const double d_i = derivative(m.components[l*2+j], x, y, i, h);
      const double d_j = derivative(m.components[l*2+i], x, y, j, h);
      const double d_l = derivative(m.components[i*2+j], x, y, l, h);
      value += 0.5 * inverse[k*2+l] * (d_i + d_j - d_l);
    }
    result[(k*2+i)*2+j] = value;
  }
  return result;
}

CurvedExecutionResult CurvedGeometryExecutor::laplace_beltrami(const CurvedGeometryCase& geometry, const Grid& g) const {
  CurvedExecutionResult result; result.geometry_id = geometry.id; result.output.kind = NumericObject::Kind::Scalar; result.output.grid = g; result.output.components = 1; result.output.values.assign(g.nx*g.ny*g.nz, 0.0);
  double min_det = std::numeric_limits<double>::infinity(), max_gamma = 0.0;
  for (int y=0;y<g.ny;++y) for (int x=0;x<g.nx;++x) {
    const double X=geometry.chart.origin[0]+x*g.step, Y=geometry.chart.origin[1]+y*g.step;
    min_det = std::min(min_det, determinant(geometry.metric,X,Y));
    for (const double value : christoffel(geometry.metric,X,Y)) if (std::isfinite(value)) max_gamma = std::max(max_gamma,std::abs(value));
  }
  if (min_det <= 0.0 || !std::isfinite(min_det)) { result.reason = "metric determinant is non-positive or singular"; return result; }
  const auto flux = [&](int x, int y, int axis) {
    x=std::clamp(x,0,g.nx-1); y=std::clamp(y,0,g.ny-1);
    const double X=geometry.chart.origin[0]+x*g.step, Y=geometry.chart.origin[1]+y*g.step;
    const double det=std::abs(determinant(geometry.metric,X,Y)); const auto inverse=inverse_metric(geometry.metric,X,Y);
    const double df0=derivative(geometry.scalar,X,Y,0,g.step), df1=derivative(geometry.scalar,X,Y,1,g.step);
    return std::sqrt(det) * (inverse[axis*2]*df0 + inverse[axis*2+1]*df1);
  };
  for (int y=0;y<g.ny;++y) for (int x=0;x<g.nx;++x) {
    const double X=geometry.chart.origin[0]+x*g.step, Y=geometry.chart.origin[1]+y*g.step; const double det=std::abs(determinant(geometry.metric,X,Y));
    const int xm=std::max(0,x-1), xp=std::min(g.nx-1,x+1), ym=std::max(0,y-1), yp=std::min(g.ny-1,y+1);
    const double dx=(flux(xp,y,0)-flux(xm,y,0))/((xp==xm)?g.step:(xp-xm)*g.step); const double dy=(flux(x,yp,1)-flux(x,ym,1))/((yp==ym)?g.step:(yp-ym)*g.step);
    result.output.values[at(g,x,y,0,0,1)] = (dx+dy)/std::sqrt(det);
  }
  double analytic_error=0.0; for(int y=1;y<g.ny-1;++y) for(int x=1;x<g.nx-1;++x){const double X=geometry.chart.origin[0]+x*g.step,Y=geometry.chart.origin[1]+y*g.step;analytic_error=std::max(analytic_error,std::abs(result.output.values[at(g,x,y,0,0,1)]-geometry.analytic_laplacian(X,Y)));}
  result.supported=true; result.min_determinant=min_det; result.max_christoffel=max_gamma; result.analytic_error=analytic_error; result.residual=analytic_error; result.reason="metric-aware Laplace-Beltrami execution"; return result;
}

std::array<double,4> CurvedGeometryExecutor::covariant_derivative(const Metric2D& m, const std::array<std::function<double(double,double)>,2>& v, double x, double y, double h) const {
  const auto gamma=christoffel(m,x,y,h); std::array<double,4> result{};
  for (int i=0;i<2;++i) for (int j=0;j<2;++j) { double value=derivative(v[j],x,y,i,h); for(int k=0;k<2;++k) value += gamma[(j*2+i)*2+k]*v[k](x,y); result[i*2+j]=value; }
  return result;
}

std::vector<CurvedGeometryCase> CurvedGeometryExecutor::reference_suite() const {
  const auto identity=[](double x,double y){return x*x+y*y;};
  Metric2D euclidean{"euclidean_cartesian", {[](double,double){return 1.0;},[](double,double){return 0.0;},[](double,double){return 0.0;},[](double,double){return 1.0;}}, true};
  Metric2D polar{"flat_polar", {[](double,double){return 1.0;},[](double,double){return 0.0;},[](double,double){return 0.0;},[](double r,double){return r*r;}}, true};
  Metric2D curved{"conformal_curved_2d", {[](double x,double y){return std::exp(0.2*(x*x+y*y));},[](double,double){return 0.0;},[](double,double){return 0.0;},[](double x,double y){return std::exp(0.2*(x*x+y*y));}}, true};
  CurvedGeometryCase flat_cartesian{"flat_cartesian","flat Euclidean metric",{"cartesian",{0,0},{}},euclidean,identity,[](double,double){return 4.0;},false};
  CurvedGeometryCase flat_polar{"flat_polar","flat metric in polar coordinates",{"polar",{1,0},{}},polar,[](double r,double){return r*r;},[](double,double){return 4.0;},false};
  CurvedGeometryCase curved_case{"curved_conformal","nonzero-curvature conformal 2D metric",{"curved_chart",{0,0},{}},curved,identity,[](double x,double y){return 4.0*std::exp(-0.2*(x*x+y*y));},true};
  return {flat_cartesian,flat_polar,curved_case};
}

CoordinateConsistencyResult CurvedGeometryExecutor::coordinate_consistency(const Grid& g) const {
  const auto cases=reference_suite(); CoordinateConsistencyResult result; result.first_geometry=cases[0].id; result.second_geometry=cases[1].id; const auto first=laplace_beltrami(cases[0],g); const auto second=laplace_beltrami(cases[1],g); double e1=0,e2=0,d=0;
  for(int y=1;y<g.ny-1;++y) for(int x=1;x<g.nx-1;++x){const double a=first.output.values[at(g,x,y,0,0,1)],b=second.output.values[at(g,x,y,0,0,1)];e1=std::max(e1,std::abs(a-4));e2=std::max(e2,std::abs(b-4));d=std::max(d,std::abs(a-b));}
  result.first_error=e1;result.second_error=e2;result.discrepancy=d;result.passed=first.supported&&second.supported&&d<0.2;result.explanation="same scalar Laplacian reference evaluated in two coordinate representations";return result;
}
}
