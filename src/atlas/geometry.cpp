#include "opforge/atlas/geometry.hpp"
#include <algorithm>

namespace opforge::atlas {
namespace {
bool has(const std::vector<std::string>& values, const std::string& value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}
}

GeometryCatalog::GeometryCatalog() {
  regimes_ = {
    {"euclidean_flat", "Flat Euclidean", GeometryKind::EuclideanFlat, -1, MetricType::Euclidean, false, false, false, true, BoundaryType::Boundaryless, RegularityLevel::Smooth, "flat metric space without boundary"},
    {"euclidean_dirichlet", "Euclidean Dirichlet", GeometryKind::EuclideanFlat, -1, MetricType::Euclidean, false, false, false, true, BoundaryType::Dirichlet, RegularityLevel::C2, "flat domain with prescribed boundary values"},
    {"riemannian_boundaryless", "Boundaryless Riemannian", GeometryKind::Riemannian, -1, MetricType::Riemannian, false, true, true, false, BoundaryType::Boundaryless, RegularityLevel::Smooth, "Riemannian manifold with connection and curvature metadata"},
    {"riemannian_boundary", "Riemannian with boundary", GeometryKind::Riemannian, -1, MetricType::Riemannian, false, true, true, false, BoundaryType::Mixed, RegularityLevel::Smooth, "Riemannian manifold with boundary conditions"},
    {"lorentzian", "Pseudo-Riemannian", GeometryKind::PseudoRiemannian, -1, MetricType::Indefinite, false, true, true, false, BoundaryType::Boundaryless, RegularityLevel::Smooth, "indefinite metric regime"},
    {"discrete_complex", "Discrete complex", GeometryKind::Discrete, -1, MetricType::DiscreteWeighted, true, false, false, true, BoundaryType::DiscreteBoundary, RegularityLevel::Discrete, "chains and cochains on a discrete complex"},
    {"graph", "Graph geometry", GeometryKind::Graph, -1, MetricType::GraphLaplacian, false, false, false, true, BoundaryType::DiscreteBoundary, RegularityLevel::Discrete, "graph and weighted graph operators"}
  };
}

const GeometryRegime* GeometryCatalog::find(const std::string& id) const {
  const auto it = std::find_if(regimes_.begin(), regimes_.end(), [&](const auto& value) { return value.id == id; });
  return it == regimes_.end() ? nullptr : &*it;
}

GeometryRequirements requirements(const OperatorSignature& signature) {
  GeometryRequirements result;
  const auto structures = signature.required_structures;
  const auto explicit_requirements = signature.geometry_requirements;
  auto required = [&](const std::string& key) { return has(structures, key) || has(explicit_requirements, key); };
  result.metric = required("metric"); result.orientation = required("orientation");
  result.connection = required("connection") || required("levi_civita_connection");
  result.curvature = required("curvature"); result.inner_product = required("inner_product");
  result.volume_form = required("volume_form"); result.boundary_data = required("boundary");
  for (const auto& constraint : signature.dimension_constraints) {
    if (constraint == "3" || constraint == "dimension=3") result.minimum_dimension = 3;
  }
  return result;
}

GeometryCheck GeometryCatalog::check(const OperatorSignature& signature, const GeometryRegime& regime) const {
  const auto needed = requirements(signature);
  GeometryCheck result;
  if (needed.minimum_dimension >= 0 && (regime.dimension >= 0 && regime.dimension < needed.minimum_dimension)) {
    result.missing.push_back("dimension");
  }
  if (needed.metric && regime.metric == MetricType::None) result.missing.push_back("metric");
  if (needed.orientation && !regime.orientation) result.missing.push_back("orientation");
  if (needed.connection && !regime.connection) result.missing.push_back("connection");
  if (needed.curvature && !regime.curvature) result.missing.push_back("curvature");
  if (needed.boundary_data && regime.boundary == BoundaryType::Boundaryless) result.missing.push_back("boundary_data");
  if (regime.kind == GeometryKind::Discrete && signature.differential_order > 0 && !signature.discrete)
    result.missing.push_back("discrete_realization");
  result.applicable = result.missing.empty();
  result.reason = result.applicable ? "requirements satisfied" : "missing geometry requirements";
  if (needed.metric) result.assumptions.push_back("metric=" + std::string(to_string(regime.metric)));
  if (needed.orientation) result.assumptions.push_back("orientation=" + std::string(regime.orientation ? "oriented" : "absent"));
  result.assumptions.push_back("boundary=" + std::string(to_string(regime.boundary)));
  result.assumptions.push_back("regularity=" + std::string(to_string(regime.regularity)));
  return result;
}

std::vector<GeometryBridge> GeometryCatalog::bridges(const Atlas& atlas) const {
  std::vector<GeometryBridge> result;
  for (const auto* op : atlas.all()) {
    for (const auto& relation : op->relations) {
      if (relation.kind == RelationKind::ContinuousAnalog || relation.kind == RelationKind::DiscreteAnalog ||
          relation.kind == RelationKind::Generalizes || relation.kind == RelationKind::TransformCorrespondence) {
        const auto* target = atlas.find(relation.target_id);
        if (!target) continue;
        const bool continuous_discrete = op->signature.continuous != target->signature.continuous ||
                                         op->signature.discrete != target->signature.discrete;
        result.push_back({op->id + "->" + target->id, op->signature.domain.id, target->signature.domain.id,
                          to_string(relation.kind), continuous_discrete ? "representation_change" : "shared_geometry",
                          !continuous_discrete || relation.kind == RelationKind::DiscreteAnalog});
      }
    }
  }
  return result;
}

const char* to_string(GeometryKind value) { switch (value) { case GeometryKind::EuclideanFlat:return "euclidean_flat"; case GeometryKind::Riemannian:return "riemannian"; case GeometryKind::PseudoRiemannian:return "pseudo_riemannian"; case GeometryKind::Manifold:return "manifold"; case GeometryKind::Discrete:return "discrete"; case GeometryKind::Graph:return "graph"; } return "unknown"; }
const char* to_string(MetricType value) { switch (value) { case MetricType::None:return "none"; case MetricType::Euclidean:return "euclidean"; case MetricType::Riemannian:return "riemannian"; case MetricType::Indefinite:return "indefinite"; case MetricType::DiscreteWeighted:return "discrete_weighted"; case MetricType::GraphLaplacian:return "graph_laplacian"; } return "unknown"; }
const char* to_string(BoundaryType value) { switch (value) { case BoundaryType::Boundaryless:return "boundaryless"; case BoundaryType::Dirichlet:return "dirichlet"; case BoundaryType::Neumann:return "neumann"; case BoundaryType::Periodic:return "periodic"; case BoundaryType::Mixed:return "mixed"; case BoundaryType::DiscreteBoundary:return "discrete_boundary"; } return "unknown"; }
const char* to_string(RegularityLevel value) { switch (value) { case RegularityLevel::Discrete:return "discrete"; case RegularityLevel::PiecewiseSmooth:return "piecewise_smooth"; case RegularityLevel::C0:return "C0"; case RegularityLevel::C1:return "C1"; case RegularityLevel::C2:return "C2"; case RegularityLevel::Smooth:return "smooth"; case RegularityLevel::Sobolev:return "sobolev"; } return "unknown"; }
}
