#include "opforge/synthesis/registry.hpp"

#include "opforge/synthesis/candidate.hpp"

#include <algorithm>

namespace opforge::synthesis {
namespace {

bool has(const atlas::Atlas& atlas, const std::string& id) { return atlas.find(id) != nullptr; }

KnownConstruction make(const std::string& id, const std::string& name,
                       const std::string& role, atlas::ExpressionPtr expression,
                       const std::string& related, const std::string& context,
                       std::vector<std::string> assumptions) {
  KnownConstruction construction;
  construction.id = id;
  construction.name = name;
  construction.role = role;
  construction.expression = std::move(expression);
  construction.related_operator = related;
  construction.decomposition_context = context;
  construction.provenance = "curated_construction_registry";
  construction.assumptions = std::move(assumptions);
  construction.evidence.push_back({id + ".curated", "curated_definition", "construction_registry", "0.3",
                                   "2026-08-15", id, "registered", "", -1});
  return construction;
}

}  // namespace

void KnownConstructionRegistry::add(KnownConstruction construction) {
  if (!find(construction.id)) constructions_.push_back(std::move(construction));
}

const KnownConstruction* KnownConstructionRegistry::find(const std::string& id) const {
  const auto it = std::find_if(constructions_.begin(), constructions_.end(),
                               [&](const auto& construction) { return construction.id == id; });
  return it == constructions_.end() ? nullptr : &*it;
}

const KnownConstruction* KnownConstructionRegistry::equivalent_to(const atlas::ExpressionPtr& expression,
                                                                   const atlas::Atlas& atlas) const {
  const auto wanted = canonical(expression);
  for (const auto& construction : constructions_) {
    if (canonical(construction.expression) == wanted) return &construction;
    if (!construction.related_operator.empty() && atlas.find(construction.related_operator) &&
        canonical(atlas::Expression::ref(construction.related_operator)) == wanted)
      return &construction;
  }
  return nullptr;
}

KnownConstructionRegistry KnownConstructionRegistry::from_atlas(const atlas::Atlas& atlas) {
  KnownConstructionRegistry registry;

  for (const auto& identity : atlas.identities()) {
    if (!identity.executable_equality) continue;
    if (identity.left && identity.right && identity.left->kind == atlas::Expression::Kind::Composition &&
        identity.right->kind == atlas::Expression::Kind::OperatorReference) {
      registry.add(make("identity." + identity.id, identity.name, "identity_rewrite", identity.left,
                        identity.right->value, "Atlas identity", identity.assumptions));
    }
  }

  if (has(atlas, "op.divergence") && has(atlas, "op.gradient") && has(atlas, "op.laplacian")) {
    registry.add(make("construction.grad_div", "Gradient-divergence", "factorization",
                      atlas::Expression::composition(atlas::Expression::ref("op.divergence"),
                                                      atlas::Expression::ref("op.gradient")),
                      "op.laplacian", "vector calculus scalar Laplacian",
                      {"Euclidean metric", "C2 regularity", "dimension=3"}));
  }
  if (has(atlas, "op.jacobian") && has(atlas, "op.gradient") && has(atlas, "op.hessian")) {
    registry.add(make("construction.hessian", "Hessian as Jacobian of gradient", "decomposition_component",
                      atlas::Expression::composition(atlas::Expression::ref("op.jacobian"),
                                                      atlas::Expression::ref("op.gradient")),
                      "op.hessian", "second derivative tensor construction",
                      {"Euclidean coordinate identification", "C2 regularity"}));
  }
  if (has(atlas, "op.curl.3d")) {
    registry.add(make("construction.curl_curl", "Curl-curl component", "decomposition_component",
                      atlas::Expression::composition(atlas::Expression::ref("op.curl.3d"),
                                                      atlas::Expression::ref("op.curl.3d")),
                      "", "vector calculus vector Laplacian decomposition",
                      {"oriented Euclidean R3", "C2 regularity"}));
  }
  if (has(atlas, "form.de_rham_laplacian") && has(atlas, "form.exterior_derivative")) {
    registry.add(make("construction.hodge_laplacian", "Hodge Laplacian decomposition", "decomposition",
                      atlas::Expression::ref("form.de_rham_laplacian"), "form.de_rham_laplacian",
                      "d delta + delta d under Hodge-star convention",
                      {"Riemannian metric", "orientation", "sign convention explicit"}));
  }
  if (has(atlas, "la.symmetric_projection") && has(atlas, "la.skew_projection")) {
    registry.add(make("construction.symmetric_skew", "Symmetric/skew decomposition", "decomposition",
                      atlas::Expression::direct_sum(atlas::Expression::ref("la.symmetric_projection"),
                                                     atlas::Expression::ref("la.skew_projection")),
                      "", "matrix decomposition",
                      {"finite-dimensional square matrix", "characteristic not 2"}));
  }
  if (has(atlas, "transform.fourier") && has(atlas, "op.gradient")) {
    registry.add(make("construction.fourier_derivative", "Fourier differentiation correspondence",
                      "transform_correspondence", atlas::Expression::ref("transform.fourier"),
                      "transform.fourier", "differentiation becomes multiplication in frequency space",
                      {"appropriate transform convention", "sufficient decay or distributional interpretation"}));
  }
  return registry;
}

}  // namespace opforge::synthesis
