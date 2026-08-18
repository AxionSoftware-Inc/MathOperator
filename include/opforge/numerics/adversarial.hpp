#pragma once

#include "opforge/numerics/executor.hpp"
#include "opforge/research/evaluation.hpp"
#include <string>
#include <vector>

namespace opforge::numerics {
enum class FieldFamily { Polynomial, Trigonometric, Bump, SmoothBasis, DivergenceFree, CurlFree, BoundarySensitive };
struct GeneratedField { std::string id, regime, construction; FieldFamily family{FieldFamily::Polynomial}; unsigned seed{0}; NumericObject value; bool expected_identity{true}; double expected_scale{1.0}; };
class AdversarialGenerator {
public:
  std::vector<GeneratedField> generate(const Grid&, FieldFamily, unsigned seed, int limit = 4) const;
  std::vector<research::TestCase> property_cases(unsigned seed, int limit = 16) const;
};
const char* to_string(FieldFamily);
} // namespace opforge::numerics
