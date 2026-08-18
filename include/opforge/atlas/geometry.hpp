#pragma once

#include "opforge/atlas/model.hpp"
#include <string>
#include <vector>

namespace opforge::atlas {

enum class GeometryKind { EuclideanFlat, Riemannian, PseudoRiemannian, Manifold, Discrete, Graph };
enum class MetricType { None, Euclidean, Riemannian, Indefinite, DiscreteWeighted, GraphLaplacian };
enum class BoundaryType { Boundaryless, Dirichlet, Neumann, Periodic, Mixed, DiscreteBoundary };
enum class RegularityLevel { Discrete, PiecewiseSmooth, C0, C1, C2, Smooth, Sobolev };

struct GeometryRegime {
  std::string id, name;
  GeometryKind kind{GeometryKind::EuclideanFlat};
  int dimension{-1};
  MetricType metric{MetricType::None};
  bool orientation{false}, curvature{false}, connection{false}, coordinate_dependent{true};
  BoundaryType boundary{BoundaryType::Boundaryless};
  RegularityLevel regularity{RegularityLevel::Smooth};
  std::string description;
};

struct GeometryRequirements {
  bool metric{false}, orientation{false}, connection{false}, curvature{false};
  bool inner_product{false}, volume_form{false}, boundary_data{false};
  int minimum_dimension{-1};
  std::vector<RegularityLevel> allowed_regularity;
};

struct GeometryCheck {
  bool applicable{false};
  std::vector<std::string> missing, assumptions;
  std::string reason;
};

struct GeometryBridge {
  std::string id, source_space, target_space, relation, condition;
  bool verified{false};
};

class GeometryCatalog {
public:
  GeometryCatalog();
  const GeometryRegime* find(const std::string& id) const;
  const std::vector<GeometryRegime>& all() const { return regimes_; }
  GeometryCheck check(const OperatorSignature&, const GeometryRegime&) const;
  std::vector<GeometryBridge> bridges(const Atlas&) const;
private:
  std::vector<GeometryRegime> regimes_;
};

GeometryRequirements requirements(const OperatorSignature&);
const char* to_string(GeometryKind);
const char* to_string(MetricType);
const char* to_string(BoundaryType);
const char* to_string(RegularityLevel);

} // namespace opforge::atlas
