#include "opforge/numerics/adversarial.hpp"
#include <algorithm>
#include <cmath>

namespace opforge::numerics {
namespace {
int at(const Grid& g, int x, int y, int z, int c, int components) { x = std::clamp(x, 0, g.nx - 1); y = std::clamp(y, 0, g.ny - 1); z = std::clamp(z, 0, g.nz - 1); return (((z * g.ny + y) * g.nx + x) * components) + c; }
NumericObject vector_grid(const Grid& g) { NumericObject o; o.kind = NumericObject::Kind::Vector; o.grid = g; o.components = g.dimension; o.values.assign(g.nx * g.ny * g.nz * o.components, 0.0); return o; }
}
std::vector<GeneratedField> AdversarialGenerator::generate(const Grid& g, FieldFamily family, unsigned seed, int limit) const {
  std::vector<GeneratedField> result;
  for (int sample = 0; sample < limit; ++sample) {
    GeneratedField field; field.id = "field-" + std::string(to_string(family)) + "-" + std::to_string(sample); field.family = family; field.seed = seed + static_cast<unsigned>(sample); field.regime = "euclidean_flat";
    if (family == FieldFamily::DivergenceFree || family == FieldFamily::BoundarySensitive) {
      field.value = vector_grid(g); field.construction = family == FieldFamily::DivergenceFree ? "curl_of_polynomial_potential" : "boundary_layer_vector";
      for (int z = 0; z < g.nz; ++z) for (int y = 0; y < g.ny; ++y) for (int x = 0; x < g.nx; ++x) { const double X = x * g.step, Y = y * g.step, Z = z * g.step; const double layer = family == FieldFamily::BoundarySensitive ? std::exp(-8.0 * X) : 1.0; field.value.values[at(g, x, y, z, 0, 3)] = -Y * layer; field.value.values[at(g, x, y, z, 1, 3)] = X * layer; field.value.values[at(g, x, y, z, 2, 3)] = (Z + sample * 0.1) * layer; }
    } else {
      field.value = scalar_grid(g, [family, sample](double x, double y, double z) { const double phase = static_cast<double>(sample + 1) * 0.17; if (family == FieldFamily::Trigonometric) return std::sin(x + phase) * std::cos(y - phase) + std::sin(z); if (family == FieldFamily::Bump) { const double r = (x - 0.5) * (x - 0.5) + (y - 0.5) * (y - 0.5) + (z - 0.5) * (z - 0.5); return std::exp(-12.0 * r); } if (family == FieldFamily::SmoothBasis) return std::cos(x) + 0.5 * std::cos(2.0 * y) + 0.25 * std::sin(3.0 * z); return (x + phase) * (x + phase) + 0.5 * y * y + 0.25 * z * z; }); field.construction = std::string(to_string(family)) + "_basis_combination";
    }
    result.push_back(std::move(field));
  }
  return result;
}
std::vector<research::TestCase> AdversarialGenerator::property_cases(unsigned seed, int limit) const {
  std::vector<research::TestCase> result; const auto families = {FieldFamily::Polynomial, FieldFamily::Trigonometric, FieldFamily::Bump, FieldFamily::BoundarySensitive}; int index = 0;
  for (const auto family : families) for (const auto& field : generate(Grid{3, 8, 8, 8, 1.0 / 7.0}, family, seed, 2)) { research::TestCase test; test.id = field.id; test.adversarial = family == FieldFamily::Bump || family == FieldFamily::BoundarySensitive; test.values = field.value.values; result.push_back(std::move(test)); if (++index >= limit) return result; }
  return result;
}
const char* to_string(FieldFamily value) { switch (value) { case FieldFamily::Polynomial:return "polynomial"; case FieldFamily::Trigonometric:return "trigonometric"; case FieldFamily::Bump:return "bump"; case FieldFamily::SmoothBasis:return "smooth_basis"; case FieldFamily::DivergenceFree:return "divergence_free"; case FieldFamily::CurlFree:return "curl_free"; case FieldFamily::BoundarySensitive:return "boundary_sensitive"; } return "unknown"; }
}
