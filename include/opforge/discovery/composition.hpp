#pragma once
#include "opforge/atlas/model.hpp"
#include <string>
namespace opforge::discovery {
struct CompositionResult {
  bool valid{false}; std::string code, reason; atlas::OperatorSignature signature;
};
CompositionResult compose(const atlas::OperatorRecord& outer,
                          const atlas::OperatorRecord& inner,
                          const atlas::Atlas& atlas);
CompositionResult infer(const atlas::ExpressionPtr& expression, const atlas::Atlas& atlas);
std::string expression_text(const atlas::ExpressionPtr& expression);
std::string export_graphviz(const atlas::Atlas& atlas);
std::string export_json(const atlas::Atlas& atlas);
atlas::Atlas import_json(const std::string& json);
struct TraceStep { std::string step, detail; };
struct RediscoveryTrace { std::string candidate, result, matched_operator, verification; std::vector<TraceStep> steps; };
RediscoveryTrace rediscover_div_grad(const atlas::Atlas& atlas);
RediscoveryTrace rediscover_curl_grad(const atlas::Atlas& atlas);
RediscoveryTrace rediscover_div_curl(const atlas::Atlas& atlas);
std::string trace_text(const RediscoveryTrace& trace);
std::string trace_json(const RediscoveryTrace& trace);
}
