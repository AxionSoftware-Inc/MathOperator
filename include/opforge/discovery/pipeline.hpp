#pragma once
#include "opforge/atlas/model.hpp"
#include <string>
#include <vector>
namespace opforge::discovery {
struct Candidate { std::string id, rationale; atlas::OperatorRecord proposed_operator; };
struct CheckResult { bool passed; std::string check, explanation; };
class VerificationPipeline {
public: std::vector<CheckResult> verify(const atlas::OperatorRecord&, const atlas::Atlas&) const;
};
class RediscoveryEngine {
public: std::vector<Candidate> discover(const atlas::Atlas&) const;
};
}
