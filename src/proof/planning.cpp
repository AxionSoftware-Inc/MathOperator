#include "opforge/proof/planning.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace opforge::proof {
namespace {

std::string token(const std::string& value) {
  return std::to_string(value.size()) + ":" + value;
}

std::string list(const std::string& tag, const std::vector<std::string>& values) {
  std::ostringstream out;
  out << tag << "[";
  for (const auto& value : values) out << token(value);
  out << "]";
  return out.str();
}

std::string canonical_constraints(const std::vector<semantic::Constraint>& constraints) {
  std::vector<std::string> values;
  values.reserve(constraints.size());
  for (const auto& constraint : constraints) values.push_back(constraint.canonical());
  std::sort(values.begin(), values.end());
  return list("constraints", values);
}

std::string canonical_judgments(const std::vector<Judgment>& judgments) {
  std::vector<std::string> values;
  values.reserve(judgments.size());
  for (const auto& judgment : judgments) values.push_back(judgment.canonical());
  return list("judgments", values);
}

bool is_terminal_discharge(ProofObligationStatus status) {
  return status == ProofObligationStatus::DischargedTrustedFact ||
         status == ProofObligationStatus::DischargedStructuralDerivation ||
         status == ProofObligationStatus::DischargedSymbolicCertificate ||
         status == ProofObligationStatus::DischargedFormalCertificate;
}

bool is_non_numeric_discharge(ProofObligationStatus status) {
  return is_terminal_discharge(status);
}

bool evidence_satisfies(EvidenceLevel required, EvidenceLevel available) {
  // This is an explicit compatibility relation, not a numeric ranking.
  switch (required) {
    case EvidenceLevel::Structural:
      return available == EvidenceLevel::Structural || available == EvidenceLevel::TrustedFact ||
             available == EvidenceLevel::Symbolic || available == EvidenceLevel::Formal;
    case EvidenceLevel::TrustedFact:
      return available == EvidenceLevel::TrustedFact || available == EvidenceLevel::Symbolic ||
             available == EvidenceLevel::Formal;
    case EvidenceLevel::Symbolic:
      return available == EvidenceLevel::Symbolic || available == EvidenceLevel::Formal;
    case EvidenceLevel::Formal:
      return available == EvidenceLevel::Formal;
    case EvidenceLevel::NumericalSupportOnly:
      return available == EvidenceLevel::NumericalSupportOnly;
  }
  return false;
}

EvidenceLevel evidence_level_for_status(ProofObligationStatus status) {
  switch (status) {
    case ProofObligationStatus::DischargedTrustedFact: return EvidenceLevel::TrustedFact;
    case ProofObligationStatus::DischargedStructuralDerivation: return EvidenceLevel::Structural;
    case ProofObligationStatus::DischargedSymbolicCertificate: return EvidenceLevel::Symbolic;
    case ProofObligationStatus::DischargedFormalCertificate: return EvidenceLevel::Formal;
    case ProofObligationStatus::NumericallySupported: return EvidenceLevel::NumericalSupportOnly;
    default: return EvidenceLevel::Structural;
  }
}

ProofObligationStatus status_for_evidence(EvidenceLevel level, bool trusted_fact) {
  if (level == EvidenceLevel::NumericalSupportOnly) return ProofObligationStatus::NumericallySupported;
  if (level == EvidenceLevel::Formal) return ProofObligationStatus::DischargedFormalCertificate;
  if (level == EvidenceLevel::Symbolic) return ProofObligationStatus::DischargedSymbolicCertificate;
  if (trusted_fact) return ProofObligationStatus::DischargedTrustedFact;
  return ProofObligationStatus::DischargedStructuralDerivation;
}

bool is_weak_kind(semantic::JudgmentKind kind) {
  return kind == semantic::JudgmentKind::Analogy || kind == semantic::JudgmentKind::Correspondence ||
         kind == semantic::JudgmentKind::Approximation || kind == semantic::JudgmentKind::GenericRelation;
}

bool has_nonempty_provenance(const Provenance& provenance) {
  return !provenance.entries.empty() &&
         std::all_of(provenance.entries.begin(), provenance.entries.end(), [](const auto& entry) {
           return !entry.source_id.empty() && !entry.source_kind.empty();
         });
}

bool has_evidence_type(const Judgment& judgment, const std::set<std::string>& allowed) {
  return std::any_of(judgment.evidence.begin(), judgment.evidence.end(), [&](const auto& evidence) {
    return allowed.count(evidence.type) != 0;
  });
}

Provenance provenance_one(std::string source_id, std::string source_kind, std::string version,
                          std::string detail) {
  return {{{std::move(source_id), std::move(source_kind), std::move(version), std::move(detail)}}};
}

Evidence evidence_one(std::string type, std::string checker, std::string version, std::string result) {
  Evidence evidence;
  evidence.type = std::move(type);
  evidence.checker = std::move(checker);
  evidence.version = std::move(version);
  evidence.result = std::move(result);
  evidence.refresh_id();
  return evidence;
}

Judgment normalized(Judgment judgment, const Context& context) {
  if (judgment.context_id.empty()) judgment.context_id = context.id;
  if (judgment.regime.id.empty()) judgment.regime = context.active_regime;
  judgment.refresh_id();
  return judgment;
}

Judgment condition_judgment(const Context& context, const std::string& name, const std::string& payload) {
  Judgment judgment;
  judgment.kind = semantic::JudgmentKind::GenericRelation;
  judgment.context_id = context.id;
  judgment.regime = context.active_regime;
  judgment.relation_name = name;
  judgment.legacy_payload = payload;
  judgment.status = EpistemicStatus::Unresolved;
  judgment.refresh_id();
  return judgment;
}

Judgment definedness_judgment(const Context& context, const ExpressionPtr& expression) {
  Judgment judgment;
  judgment.kind = semantic::JudgmentKind::Definedness;
  judgment.context_id = context.id;
  judgment.regime = context.active_regime;
  judgment.operands = {expression};
  judgment.status = EpistemicStatus::Unresolved;
  judgment.refresh_id();
  return judgment;
}

struct FactMatch {
  const semantic::Judgment* fact{nullptr};
  reasoning::MatchStatus match_status{reasoning::MatchStatus::NoMatch};
  bool trusted{false};
  EvidenceLevel level{EvidenceLevel::Structural};
};

class PlannerImpl {
public:
  PlannerImpl(ProofPlan& plan_value, const Theory& theory_value, const Context& context_value,
              const std::vector<ProofRule>& rules_value, const std::vector<ProofCertificate>& certificates_value,
              ProofPlanningOptions options_value)
      : plan_(plan_value), theory_(theory_value), context_(context_value), rules_(rules_value),
        certificates_(certificates_value), options_(options_value) {
    for (std::size_t index = 0; index < rules_.size(); ++index) {
      const auto& rule = rules_[index];
      if (!rule.id.empty()) rules_by_id_[rule.id] = index;
    }
  }

  void set_structural_candidate(const reasoning::SolutionCandidate* candidate) {
    candidate_ = candidate;
    if (!candidate) return;
    structural_targets_.insert(candidate->target.canonical());
  }

  void add_structural_snapshot(const reasoning::GoalSearchResult& search, const reasoning::SolutionCandidate& candidate) {
    for (const auto& state : search.forward_states) {
      if (std::find(candidate.forward_lineage.begin(), candidate.forward_lineage.end(), state.id) ==
          candidate.forward_lineage.end())
        continue;
      structural_targets_.insert(state.judgment.canonical());
      structural_targets_.insert(normalized(state.judgment, context_).canonical());
    }
    for (const auto& state : search.goal_states) {
      if (std::find(candidate.backward_lineage.begin(), candidate.backward_lineage.end(), state.id) ==
          candidate.backward_lineage.end())
        continue;
      structural_targets_.insert(state.target.canonical());
      structural_targets_.insert(normalized(state.target, state.context.id.empty() ? context_ : state.context).canonical());
      if (!state.rule_used.empty()) lineage_rule_ids_.insert(state.rule_used);
    }
    for (const auto& lineage_id : candidate.backward_lineage) {
      if (rules_by_id_.count(lineage_id) == 0 &&
          std::none_of(search.goal_states.begin(), search.goal_states.end(),
                       [&](const auto& state) { return state.id == lineage_id; })) {
        missing_lineage_rules_.insert(lineage_id);
      }
    }
  }

  SemanticId add_obligation(Judgment target, std::string label, std::string origin_id,
                            Provenance provenance) {
    target = normalized(std::move(target), context_);
    ++plan_.accounting.generated_obligations;
    const auto key = target.canonical();
    const auto found = obligation_by_key_.find(key);
    if (found != obligation_by_key_.end()) {
      ++plan_.accounting.duplicate_obligations;
      auto& obligation = plan_.obligations[found->second];
      if (!origin_id.empty() && obligation.origin_id.empty()) obligation.origin_id = std::move(origin_id);
      if (obligation.provenance.entries.empty()) obligation.provenance = std::move(provenance);
      if (obligation.required_evidence == "UNSPECIFIED") obligation.required_evidence = to_string(options_.required_evidence);
      return obligation.id;
    }
    if (options_.max_obligations != 0 && plan_.obligations.size() >= options_.max_obligations) {
      ProofObligation limited;
      limited.label = std::move(label);
      limited.target = std::move(target);
      limited.origin_id = std::move(origin_id);
      limited.provenance = std::move(provenance);
      limited.context = context_;
      limited.regime = limited.target.regime;
      limited.required_evidence = to_string(options_.required_evidence);
      limited.status = ProofObligationStatus::Unsupported;
      limited.reason = "proof-planning obligation budget ended before expansion";
      limited.refresh_id();
      plan_.obligations.push_back(limited);
      obligation_by_key_[key] = plan_.obligations.size() - 1;
      add_obligation_node(plan_.obligations.back());
      return plan_.obligations.back().id;
    }
    ProofObligation obligation;
    obligation.label = std::move(label);
    obligation.target = std::move(target);
    obligation.origin_id = std::move(origin_id);
    obligation.provenance = std::move(provenance);
    obligation.context = context_;
    obligation.regime = obligation.target.regime;
    obligation.required_evidence = to_string(options_.required_evidence);
    obligation.status = ProofObligationStatus::Unresolved;
    obligation.refresh_id();
    obligation_by_key_[key] = plan_.obligations.size();
    plan_.obligations.push_back(std::move(obligation));
    add_obligation_node(plan_.obligations.back());
    return plan_.obligations.back().id;
  }

  void add_root(const SemanticId& id) {
    if (std::find(plan_.root_obligation_ids.begin(), plan_.root_obligation_ids.end(), id) ==
        plan_.root_obligation_ids.end())
      plan_.root_obligation_ids.push_back(id);
  }

  void add_dependency(const SemanticId& parent_id, const SemanticId& child_id) {
    if (parent_id == child_id) return;
    auto* parent = obligation(parent_id);
    if (parent && std::find(parent->dependency_ids.begin(), parent->dependency_ids.end(), child_id) ==
                      parent->dependency_ids.end())
      parent->dependency_ids.push_back(child_id);
    add_edge(node_for_obligation(parent_id), node_for_obligation(child_id), ProofEdgeKind::DependsOn, {});
  }

  void expand(const SemanticId& id) {
    const auto* current = obligation(id);
    if (!current) return;
    // Adding a semantic prerequisite appends to plan_.obligations and may
    // reallocate its vector.  Keep the target by value while expanding so no
    // reference into that vector survives an append.
    const auto current_target = current->target;
    const auto key = current_target.canonical();
    const auto active = std::find(active_stack_.begin(), active_stack_.end(), key);
    if (active != active_stack_.end()) {
      std::vector<SemanticId> cycle;
      for (auto item = active; item != active_stack_.end(); ++item) cycle.push_back(*item);
      cycle.push_back(key);
      plan_.cycles.push_back(std::move(cycle));
      set_status(id, ProofObligationStatus::BlockedUnknown, "circular proof dependency detected",
                 provenance_one(id, "layer18-cycle", theory_.version, "cycle was retained and not discharged"));
      return;
    }
    if (expanded_.count(key) != 0) return;
    expanded_.insert(key);
    active_stack_.push_back(key);

    if (current_target.status == EpistemicStatus::Falsified) {
      set_status(id, ProofObligationStatus::Falsified, "target judgment is explicitly marked FALSIFIED",
                 provenance_one(current_target.id, "layer18-target", theory_.version, "falsified input"));
      active_stack_.pop_back();
      return;
    }

    generate_semantic_conditions(id, current_target);
    apply_certificates(id);
    apply_trusted_facts(id, current_target);
    if (structural_targets_.count(key) != 0 && candidate_ &&
        evidence_satisfies(options_.required_evidence, EvidenceLevel::Structural)) {
      set_status(id, ProofObligationStatus::DischargedStructuralDerivation,
                 "Layer-17 structural derivation is available at structural level",
                 provenance_one(candidate_->id, "layer17-structural-candidate", candidate_->scope.quotient_scope.theory_version,
                                "structural evidence is not a formal proof"),
                 {evidence_one("structural_derivation", "layer17", candidate_->scope.quotient_scope.theory_version,
                                "candidate")});
      add_evidence_node(id, candidate_->id, ProofObligationStatus::DischargedStructuralDerivation,
                        plan_.provenance);
    }

    const auto matching_rules = matching_rule_indices(current_target);
    for (const auto rule_index : matching_rules) apply_rule(id, rules_[rule_index]);
    active_stack_.pop_back();
  }

  void mark_missing_lineage_rules() {
    for (const auto& rule_id : missing_lineage_rules_) {
      const auto validity = condition_judgment(context_, "proof_rule_validity", rule_id);
      const auto obligation_id = add_obligation(
          validity, "validity of missing Layer-17 rule " + rule_id, rule_id,
          provenance_one(rule_id, "layer17-lineage", theory_.version,
                         "Layer-17 lineage lacks a Layer-18 proof-rule contract"));
      set_status(obligation_id, ProofObligationStatus::Unsupported,
                 "Layer-17 rule was used without explicit proof-safe provenance",
                 provenance_one(rule_id, "layer18-rule-boundary", theory_.version,
                                "search rule cannot silently become proof rule"));
    }
  }

  void finalize() {
    // Rule nodes are evaluated to a fixed point because a rule application may
    // depend on a shared obligation produced by another branch.
    for (std::size_t round = 0; round <= plan_.obligations.size() + plan_.nodes.size(); ++round) {
      bool changed = false;
      for (auto& node : plan_.nodes) {
        if (node.kind != ProofNodeKind::RuleApplication) continue;
        const auto rule_found = rules_by_id_.find(node.rule_id);
        if (rule_found == rules_by_id_.end()) continue;
        const auto& rule = rules_[rule_found->second];
        if (!rule.proof_safe) {
          changed |= update_node_status(node, ProofObligationStatus::Unsupported);
          continue;
        }
        std::vector<const ProofObligation*> dependencies;
        for (const auto& edge : plan_.edges) {
          if (edge.source_id != node.id || edge.kind != ProofEdgeKind::DependsOn) continue;
          const auto* dependency_node = node_by_id(edge.target_id);
          if (!dependency_node || dependency_node->obligation_id.empty()) continue;
          dependencies.push_back(obligation(dependency_node->obligation_id));
        }
        ProofObligationStatus next = ProofObligationStatus::DischargedStructuralDerivation;
        for (const auto* dependency : dependencies) {
          if (!dependency) continue;
          if (dependency->status == ProofObligationStatus::Falsified) {
            next = ProofObligationStatus::Falsified;
            break;
          }
          if (dependency->status == ProofObligationStatus::Contradicted) {
            next = ProofObligationStatus::Contradicted;
            break;
          }
          if (dependency->status == ProofObligationStatus::BlockedUnknown) {
            next = ProofObligationStatus::BlockedUnknown;
            break;
          }
          if (dependency->status == ProofObligationStatus::Unsupported) {
            next = ProofObligationStatus::Unsupported;
            break;
          }
          if (dependency->status == ProofObligationStatus::Unresolved) {
            next = ProofObligationStatus::Unresolved;
            break;
          }
          if (!evidence_satisfies(options_.required_evidence, evidence_level_for_status(dependency->status))) {
            next = ProofObligationStatus::Unresolved;
            break;
          }
        }
        changed |= update_node_status(node, next);
        if (next == ProofObligationStatus::DischargedStructuralDerivation && !node.obligation_id.empty()) {
          const auto* owner = obligation(node.obligation_id);
          if (owner && (owner->status == ProofObligationStatus::Unresolved ||
                        owner->status == ProofObligationStatus::BlockedUnknown)) {
            set_status(node.obligation_id, next, "explicit proof-safe rule application has all required premises",
                       rule.provenance, {evidence_one("proof_rule_application", "layer18", theory_.version, rule.id)});
            changed = true;
          }
        }
      }
      if (!changed) break;
    }
    for (const auto& cycle : plan_.cycles) {
      for (const auto& key : cycle) {
        const auto found = obligation_by_key_.find(key);
        if (found != obligation_by_key_.end() && plan_.obligations[found->second].status == ProofObligationStatus::Unresolved)
          set_status(plan_.obligations[found->second].id, ProofObligationStatus::BlockedUnknown,
                     "obligation belongs to a circular dependency", {});
      }
    }
    plan_.accounting.unique_obligations = plan_.obligations.size();
    plan_.accounting.automatically_discharged = 0;
    plan_.accounting.open = plan_.accounting.unknown = plan_.accounting.falsified = 0;
    plan_.accounting.contradicted = plan_.accounting.cyclic = plan_.accounting.unsupported = 0;
    plan_.accounting.numerically_supported = 0;
    std::set<std::string> cycle_keys;
    for (const auto& cycle : plan_.cycles) cycle_keys.insert(cycle.begin(), cycle.end());
    plan_.unresolved_obligation_ids.clear();
    for (const auto& obligation_value : plan_.obligations) {
      const auto status = obligation_value.status;
      if (is_non_numeric_discharge(status)) {
        ++plan_.accounting.automatically_discharged;
      } else if (status == ProofObligationStatus::NumericallySupported) {
        ++plan_.accounting.numerically_supported;
      } else if (status == ProofObligationStatus::Unresolved) {
        ++plan_.accounting.open;
        plan_.unresolved_obligation_ids.push_back(obligation_value.id);
      } else if (status == ProofObligationStatus::BlockedUnknown) {
        if (cycle_keys.count(obligation_value.target.canonical()) != 0)
          ++plan_.accounting.cyclic;
        else
          ++plan_.accounting.unknown;
        plan_.unresolved_obligation_ids.push_back(obligation_value.id);
      } else if (status == ProofObligationStatus::Falsified) {
        ++plan_.accounting.falsified;
        plan_.unresolved_obligation_ids.push_back(obligation_value.id);
      } else if (status == ProofObligationStatus::Contradicted) {
        ++plan_.accounting.contradicted;
        plan_.unresolved_obligation_ids.push_back(obligation_value.id);
      } else if (status == ProofObligationStatus::Unsupported) {
        ++plan_.accounting.unsupported;
        plan_.unresolved_obligation_ids.push_back(obligation_value.id);
      }
    }
    plan_.accounting.generated_obligations = plan_.accounting.unique_obligations + plan_.accounting.duplicate_obligations;
    choose_plan_status();
  }

  void initialize_candidate_root(const reasoning::SolutionCandidate* candidate) {
    if (!candidate) return;
    const auto root_id = add_obligation(candidate->target, "target structural derivation", candidate->id,
                                         provenance_one(candidate->id, "layer17-structural-candidate",
                                                        candidate->scope.quotient_scope.theory_version,
                                                        "Layer-17 candidate is input evidence only"));
    add_root(root_id);
    expand(root_id);
  }

  void initialize_direct_root(const Judgment& target) {
    const auto root_id = add_obligation(target, "target proof obligation", {},
                                        provenance_one(target.id, "layer18-target", theory_.version,
                                                       "target-only proof planning input"));
    add_root(root_id);
    expand(root_id);
  }

  void add_lineage_obligations(const reasoning::GoalSearchResult& search,
                               const reasoning::SolutionCandidate& candidate) {
    for (const auto& state : search.forward_states) {
      if (std::find(candidate.forward_lineage.begin(), candidate.forward_lineage.end(), state.id) ==
          candidate.forward_lineage.end())
        continue;
      const auto id = add_obligation(state.judgment, "forward lineage " + state.id, state.id, state.judgment.provenance);
      expand(id);
    }
    for (const auto& state : search.goal_states) {
      if (std::find(candidate.backward_lineage.begin(), candidate.backward_lineage.end(), state.id) ==
          candidate.backward_lineage.end())
        continue;
      auto state_target = state.target;
      if (state_target.context_id.empty()) state_target.context_id = state.context.id;
      if (state_target.regime.id.empty()) state_target.regime = state.context.active_regime;
      const auto id = add_obligation(state_target, "backward goal " + state.id, state.id, state.provenance);
      expand(id);
      if (!state.rule_used.empty()) {
        const auto rule_found = rules_by_id_.find(state.rule_used);
        if (rule_found == rules_by_id_.end()) missing_lineage_rules_.insert(state.rule_used);
      }
    }
    for (const auto& state : search.goal_states) {
      if (std::find(candidate.backward_lineage.begin(), candidate.backward_lineage.end(), state.id) ==
          candidate.backward_lineage.end())
        continue;
      for (const auto& condition : state.constraints) {
        auto condition_target = condition_judgment(context_, "rule_condition", condition.constraint.canonical());
        const auto condition_id = add_obligation(condition_target, "condition " + condition.constraint.canonical(),
                                                  state.id, state.provenance);
        apply_constraint_state(condition_id, condition, state.provenance);
      }
    }
  }

  void add_candidate_conditions(const reasoning::SolutionCandidate& candidate) {
    for (const auto& condition : candidate.unresolved_conditions) {
      auto target = condition_judgment(context_, "candidate_condition", condition.constraint.canonical());
      const auto id = add_obligation(target, "candidate unresolved condition", candidate.id,
                                      provenance_one(candidate.id, "layer17-constraint", theory_.version,
                                                     "candidate explicitly retained this unresolved condition"));
      apply_constraint_state(id, condition,
                             provenance_one(candidate.id, "layer17-constraint", theory_.version, "candidate constraint"));
    }
    for (const auto& [variable_id, expression] : candidate.substitution.expressions) {
      const auto id = add_obligation(definedness_judgment(context_, expression), "defined substitution " + variable_id,
                                     candidate.id,
                                     provenance_one(candidate.id, "layer17-substitution", theory_.version,
                                                    "substitution requires a typed term"));
      const auto type = semantic::type_check(expression, theory_, context_);
      if (type.status == semantic::TypeCheckStatus::Valid)
        set_status(id, ProofObligationStatus::DischargedStructuralDerivation, "Layer-15 type checker accepted substitution",
                   provenance_one("layer15.type_check", "layer15-semantic-core", theory_.version, type.type.canonical()),
                   {evidence_one("type_checked", "layer15", theory_.version, "valid")});
      else if (type.status == semantic::TypeCheckStatus::Invalid)
        set_status(id, ProofObligationStatus::Falsified, type.reason,
                   provenance_one("layer15.type_check", "layer15-semantic-core", theory_.version, type.reason));
      else
        set_status(id, ProofObligationStatus::BlockedUnknown, type.reason,
                   provenance_one("layer15.type_check", "layer15-semantic-core", theory_.version, type.reason));
    }
  }

  void apply_constraint_state(const SemanticId& id, const reasoning::ConstraintState& state,
                              const Provenance& provenance) {
    switch (state.status) {
      case reasoning::ConstraintStatus::Satisfied:
        set_status(id, ProofObligationStatus::DischargedStructuralDerivation, "Layer-17 recorded condition as satisfied",
                   provenance, {evidence_one("structured_condition", "layer17", theory_.version, "satisfied")});
        break;
      case reasoning::ConstraintStatus::Violated:
        set_status(id, ProofObligationStatus::Contradicted, state.reason,
                   provenance_one(id, "layer18-condition", theory_.version, state.reason));
        break;
      case reasoning::ConstraintStatus::Unknown:
        set_status(id, ProofObligationStatus::BlockedUnknown, state.reason,
                   provenance_one(id, "layer18-condition", theory_.version, state.reason));
        break;
    }
  }

  void replay_statuses(const ProofPlan& previous) {
    // Existing topology and identity are retained.  Evidence-backed statuses
    // are recomputed; structural statuses are retained only when the source
    // plan actually records a Layer-17 candidate.
    for (auto& obligation_value : plan_.obligations) {
      obligation_value.required_evidence = to_string(options_.required_evidence);
      const auto old_found = std::find_if(previous.obligations.begin(), previous.obligations.end(),
                                          [&](const auto& item) { return item.id == obligation_value.id; });
      if (old_found == previous.obligations.end()) continue;
      obligation_value.status = ProofObligationStatus::Unresolved;
      obligation_value.reason = "replay requires current evidence";
      obligation_value.evidence.clear();
      if (old_found->status == ProofObligationStatus::DischargedStructuralDerivation &&
          !previous.structural_candidate_id.empty()) {
        obligation_value.status = old_found->status;
        obligation_value.reason = old_found->reason;
        obligation_value.evidence = old_found->evidence;
        continue;
      }
      apply_certificates(obligation_value.id);
      apply_trusted_facts(obligation_value.id, obligation_value.target);
      if (old_found->status == ProofObligationStatus::Falsified || old_found->status == ProofObligationStatus::Contradicted)
        obligation_value.status = old_found->status;
    }
  }

private:
  ProofPlan& plan_;
  const Theory& theory_;
  const Context& context_;
  const std::vector<ProofRule>& rules_;
  const std::vector<ProofCertificate>& certificates_;
  ProofPlanningOptions options_;
  const reasoning::SolutionCandidate* candidate_{nullptr};
  std::map<std::string, std::size_t> obligation_by_key_;
  std::map<std::string, std::size_t> rules_by_id_;
  std::set<std::string> structural_targets_;
  std::set<std::string> lineage_rule_ids_;
  std::set<std::string> missing_lineage_rules_;
  std::set<std::string> expanded_;
  std::vector<std::string> active_stack_;

  ProofObligation* obligation(const SemanticId& id) {
    const auto found = std::find_if(plan_.obligations.begin(), plan_.obligations.end(),
                                    [&](const auto& item) { return item.id == id; });
    return found == plan_.obligations.end() ? nullptr : &*found;
  }
  const ProofObligation* obligation(const SemanticId& id) const {
    const auto found = std::find_if(plan_.obligations.begin(), plan_.obligations.end(),
                                    [&](const auto& item) { return item.id == id; });
    return found == plan_.obligations.end() ? nullptr : &*found;
  }
  ProofPlanNode* node_by_id(const SemanticId& id) {
    const auto found = std::find_if(plan_.nodes.begin(), plan_.nodes.end(),
                                    [&](const auto& item) { return item.id == id; });
    return found == plan_.nodes.end() ? nullptr : &*found;
  }
  const ProofPlanNode* node_by_id(const SemanticId& id) const {
    const auto found = std::find_if(plan_.nodes.begin(), plan_.nodes.end(),
                                    [&](const auto& item) { return item.id == id; });
    return found == plan_.nodes.end() ? nullptr : &*found;
  }
  SemanticId node_for_obligation(const SemanticId& obligation_id) const {
    const auto found = std::find_if(plan_.nodes.begin(), plan_.nodes.end(),
                                    [&](const auto& item) {
                                      return item.kind == ProofNodeKind::Obligation &&
                                             item.obligation_id == obligation_id;
                                    });
    return found == plan_.nodes.end() ? SemanticId{} : found->id;
  }

  void add_obligation_node(const ProofObligation& obligation_value) {
    ProofPlanNode node;
    node.kind = ProofNodeKind::Obligation;
    node.obligation_id = obligation_value.id;
    node.context_id = obligation_value.context.id;
    node.regime_id = obligation_value.regime.id;
    node.status = obligation_value.status;
    node.label = obligation_value.label;
    node.provenance = obligation_value.provenance;
    node.refresh_id();
    plan_.nodes.push_back(std::move(node));
  }

  void add_evidence_node(const SemanticId& obligation_id, const SemanticId& source_id,
                         ProofObligationStatus status, const Provenance& provenance) {
    ProofPlanNode node;
    node.kind = ProofNodeKind::Evidence;
    node.obligation_id = obligation_id;
    node.certificate_id = source_id;
    node.context_id = plan_.context.id;
    node.regime_id = plan_.regime.id;
    node.status = status;
    node.label = "evidence " + source_id;
    node.provenance = provenance;
    node.refresh_id();
    if (std::none_of(plan_.nodes.begin(), plan_.nodes.end(), [&](const auto& item) { return item.id == node.id; })) {
      plan_.nodes.push_back(node);
      add_edge(node.id, node_for_obligation(obligation_id), ProofEdgeKind::Supports, source_id);
    }
  }

  void add_edge(const SemanticId& source, const SemanticId& target, ProofEdgeKind kind, const SemanticId& branch) {
    if (source.empty() || target.empty()) return;
    ProofPlanEdge edge;
    edge.source_id = source;
    edge.target_id = target;
    edge.kind = kind;
    edge.branch_id = branch;
    edge.context_id = plan_.context.id;
    edge.regime_id = plan_.regime.id;
    edge.refresh_id();
    if (std::none_of(plan_.edges.begin(), plan_.edges.end(), [&](const auto& item) { return item.id == edge.id; }))
      plan_.edges.push_back(std::move(edge));
  }

  void set_status(const SemanticId& id, ProofObligationStatus status, const std::string& reason,
                  const Provenance& provenance, const std::vector<Evidence>& evidence = {}) {
    auto* value = obligation(id);
    if (!value) return;
    const bool old_is_terminal = is_terminal_discharge(value->status) ||
                                 value->status == ProofObligationStatus::NumericallySupported;
    const bool new_is_failure = status == ProofObligationStatus::Falsified ||
                                status == ProofObligationStatus::Contradicted;
    if (old_is_terminal && !new_is_failure) return;
    value->status = status;
    value->reason = reason;
    if (!provenance.entries.empty()) value->provenance = provenance;
    value->evidence = evidence;
    for (auto& node : plan_.nodes)
      if (node.kind == ProofNodeKind::Obligation && node.obligation_id == id) node.status = status;
  }

  bool update_node_status(ProofPlanNode& node, ProofObligationStatus status) {
    if (node.status == status) return false;
    node.status = status;
    return true;
  }

  std::vector<std::size_t> matching_rule_indices(const Judgment& target) const {
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < rules_.size(); ++index) {
      const auto& rule = rules_[index];
      const auto match = reasoning::match_judgment(rule.conclusion, rule.pattern_context, target, theory_, context_);
      if (match.status == reasoning::MatchStatus::Match) result.push_back(index);
    }
    std::sort(result.begin(), result.end(), [&](std::size_t left, std::size_t right) {
      return rules_[left].id < rules_[right].id;
    });
    return result;
  }

  void apply_rule(const SemanticId& obligation_id, const ProofRule& rule) {
    ProofPlanNode rule_node;
    rule_node.kind = ProofNodeKind::RuleApplication;
    rule_node.obligation_id = obligation_id;
    rule_node.rule_id = rule.id;
    rule_node.status = ProofObligationStatus::Unresolved;
    rule_node.label = rule.name;
    rule_node.provenance = rule.provenance;
    rule_node.refresh_id();
    if (std::any_of(plan_.nodes.begin(), plan_.nodes.end(), [&](const auto& item) { return item.id == rule_node.id; })) return;
    plan_.nodes.push_back(rule_node);
    add_edge(node_for_obligation(obligation_id), rule_node.id,
             options_.retain_alternatives ? ProofEdgeKind::Alternative : ProofEdgeKind::DerivedBy, rule.id);

    if (!rule.proof_safe) {
      const auto target = condition_judgment(context_, "proof_rule_validity", rule.id);
      const auto validity = add_obligation(target, "proof validity of " + rule.id, rule.id, rule.provenance);
      set_status(validity, ProofObligationStatus::Unsupported,
                 "rule has no explicit proof-safe contract", rule.provenance);
      add_edge(rule_node.id, node_for_obligation(validity), ProofEdgeKind::DependsOn, rule.id);
      return;
    }
    const auto regime = context_.active_regime.compare(rule.regime);
    if (regime == semantic::RegimeCompatibility::Incompatible) {
      update_node_status(*node_by_id(rule_node.id), ProofObligationStatus::Contradicted);
      return;
    }
    if (regime == semantic::RegimeCompatibility::Unknown) {
      update_node_status(*node_by_id(rule_node.id), ProofObligationStatus::BlockedUnknown);
      return;
    }
    const auto match = reasoning::match_judgment(rule.conclusion, rule.pattern_context,
                                                 obligation(obligation_id)->target, theory_, context_);
    if (match.status == reasoning::MatchStatus::Unknown) {
      update_node_status(*node_by_id(rule_node.id), ProofObligationStatus::BlockedUnknown);
      return;
    }
    if (match.status != reasoning::MatchStatus::Match) {
      update_node_status(*node_by_id(rule_node.id), ProofObligationStatus::Unsupported);
      return;
    }
    for (const auto& condition : rule.conditions) {
      auto target = condition_judgment(context_, "rule_condition", condition.canonical());
      const auto condition_id = add_obligation(target, "condition " + condition.canonical(), rule.id, rule.provenance);
      const auto compatibility = context_.satisfies({condition});
      if (compatibility == semantic::RegimeCompatibility::Compatible)
        set_status(condition_id, ProofObligationStatus::DischargedStructuralDerivation,
                   "context explicitly satisfies rule condition",
                   provenance_one(context_.id, "context-regime", theory_.version, condition.canonical()),
                   {evidence_one("structured_context", "layer18", theory_.version, "satisfied")});
      else if (compatibility == semantic::RegimeCompatibility::Incompatible)
        set_status(condition_id, ProofObligationStatus::Contradicted, "rule condition is incompatible with context",
                   provenance_one(rule.id, "layer18-rule-condition", theory_.version, condition.canonical()));
      else
        set_status(condition_id, ProofObligationStatus::BlockedUnknown, "rule condition is unknown in context",
                   provenance_one(rule.id, "layer18-rule-condition", theory_.version, condition.canonical()));
      add_dependency(obligation_id, condition_id);
      add_edge(rule_node.id, node_for_obligation(condition_id), ProofEdgeKind::DependsOn, rule.id);
    }
    for (const auto& premise : rule.premises) {
      auto instantiated = reasoning::instantiate_judgment(premise, match.substitution);
      const auto premise_id = add_obligation(instantiated, "premise of " + rule.id, rule.id, rule.provenance);
      add_dependency(obligation_id, premise_id);
      add_edge(rule_node.id, node_for_obligation(premise_id), ProofEdgeKind::DependsOn, rule.id);
      expand(premise_id);
    }
  }

  void generate_semantic_conditions(const SemanticId& obligation_id, const Judgment& target) {
    if (!checked_semantics_.insert(obligation_id).second) return;
    const auto regime = context_.active_regime.compare(target.regime);
    if (options_.generate_regime_obligations) {
      const auto regime_target = condition_judgment(context_, "validity_regime", target.regime.canonical());
      const auto regime_id = add_obligation(regime_target, "validity regime for " + target.id, target.id,
                                             provenance_one(target.id, "layer18-regime", theory_.version,
                                                            "regime compatibility is explicit"));
      if (regime == semantic::RegimeCompatibility::Compatible || regime == semantic::RegimeCompatibility::Equal)
        set_status(regime_id, ProofObligationStatus::DischargedStructuralDerivation,
                   "context/regime compatibility is known", provenance_one(context_.id, "context-regime", theory_.version,
                                                                             "compatible"),
                   {evidence_one("structured_regime", "layer15", theory_.version, "compatible")});
      else if (regime == semantic::RegimeCompatibility::Incompatible)
        set_status(regime_id, ProofObligationStatus::Contradicted, "target regime is incompatible with context",
                   provenance_one(target.id, "layer18-regime", theory_.version, "incompatible"));
      else
        set_status(regime_id, ProofObligationStatus::BlockedUnknown, "target/context regime overlap is unknown",
                   provenance_one(target.id, "layer18-regime", theory_.version, "unknown"));
      add_dependency(obligation_id, regime_id);
    }
    for (const auto& condition : target.side_conditions) {
      const auto condition_target = condition_judgment(context_, "side_condition", condition.canonical());
      const auto condition_id = add_obligation(condition_target, "side condition " + condition.canonical(), target.id,
                                               provenance_one(target.id, "layer18-side-condition", theory_.version,
                                                              "condition required by target judgment"));
      const auto compatibility = context_.satisfies({condition});
      if (compatibility == semantic::RegimeCompatibility::Compatible)
        set_status(condition_id, ProofObligationStatus::DischargedStructuralDerivation,
                   "context satisfies side condition", provenance_one(context_.id, "context-regime", theory_.version,
                                                                       condition.canonical()));
      else if (compatibility == semantic::RegimeCompatibility::Incompatible)
        set_status(condition_id, ProofObligationStatus::Contradicted, "side condition is incompatible",
                   provenance_one(target.id, "layer18-side-condition", theory_.version, "incompatible"));
      else
        set_status(condition_id, ProofObligationStatus::BlockedUnknown, "side condition is unknown",
                   provenance_one(target.id, "layer18-side-condition", theory_.version, "unknown"));
      add_dependency(obligation_id, condition_id);
    }
    if (options_.generate_typing_obligations) {
      for (const auto& operand : target.operands) {
        const auto type = semantic::type_check(operand, theory_, context_);
        const auto type_target = condition_judgment(context_, "type_check", operand ? operand->canonical() : "null");
        const auto type_id = add_obligation(type_target, "typing of target expression", target.id,
                                             provenance_one(target.id, "layer18-typing", theory_.version,
                                                            "expression typing is a proof prerequisite"));
        if (type.status == semantic::TypeCheckStatus::Valid)
          set_status(type_id, ProofObligationStatus::DischargedStructuralDerivation,
                     "Layer-15 type checker accepted expression", provenance_one("layer15.type_check", "layer15-semantic-core",
                                                                                   theory_.version, type.type.canonical()),
                     {evidence_one("type_checked", "layer15", theory_.version, "valid")});
        else if (type.status == semantic::TypeCheckStatus::Invalid)
          set_status(type_id, ProofObligationStatus::Falsified, type.reason,
                     provenance_one("layer15.type_check", "layer15-semantic-core", theory_.version, type.reason));
        else
          set_status(type_id, ProofObligationStatus::BlockedUnknown, type.reason,
                     provenance_one("layer15.type_check", "layer15-semantic-core", theory_.version, type.reason));
        add_dependency(obligation_id, type_id);
      }
    }
    if (target.kind == semantic::JudgmentKind::Equality) {
      const auto rewrite = semantic::rewrite_safety(target, theory_, context_);
      const auto rewrite_target = condition_judgment(context_, "rewrite_safety", target.canonical());
      const auto rewrite_id = add_obligation(rewrite_target, "rewrite safety for equality " + target.id, target.id,
                                             provenance_one(target.id, "layer18-rewrite", theory_.version,
                                                            "exact equality requires explicit rewrite safety"));
      if (rewrite.safety == semantic::RewriteSafety::Allowed)
        set_status(rewrite_id, ProofObligationStatus::DischargedStructuralDerivation,
                   "Layer-15 rewrite safety accepted equality", provenance_one("layer15.rewrite_safety", "layer15-semantic-core",
                                                                                theory_.version, rewrite.reason),
                   {evidence_one("rewrite_safety", "layer15", theory_.version, "allowed")});
      else if (rewrite.safety == semantic::RewriteSafety::Rejected)
        set_status(rewrite_id, ProofObligationStatus::Falsified, rewrite.reason,
                   provenance_one("layer15.rewrite_safety", "layer15-semantic-core", theory_.version, rewrite.reason));
      else
        set_status(rewrite_id, ProofObligationStatus::BlockedUnknown, rewrite.reason,
                   provenance_one("layer15.rewrite_safety", "layer15-semantic-core", theory_.version, rewrite.reason));
      add_dependency(obligation_id, rewrite_id);
    }
  }

  void apply_certificates(const SemanticId& obligation_id) {
    for (const auto& certificate : certificates_) {
      if (certificate.obligation_id != obligation_id || certificate.status != CertificateStatus::Accepted) continue;
      if (!has_nonempty_provenance(certificate.provenance)) continue;
      if (!evidence_satisfies(options_.required_evidence, certificate.evidence_level)) continue;
      plan_.certificates.push_back(certificate);
      auto status = status_for_evidence(certificate.evidence_level, false);
      set_status(obligation_id, status, "accepted backend-neutral certificate envelope",
                 certificate.provenance,
                 {evidence_one("certificate", certificate.backend, "layer18-envelope-v1", certificate.id)});
      add_evidence_node(obligation_id, certificate.id, status, certificate.provenance);
    }
  }

  void apply_trusted_facts(const SemanticId& obligation_id, const Judgment& target) {
    bool unknown = false;
    for (const auto& fact : theory_.facts) {
      if (fact.kind != target.kind || is_weak_kind(fact.kind)) continue;
      const auto match = reasoning::match_judgment(fact, context_, target, theory_, context_);
      if (match.status == reasoning::MatchStatus::Unknown) {
        unknown = true;
        continue;
      }
      if (match.status != reasoning::MatchStatus::Match) continue;
      if (fact.status == EpistemicStatus::Falsified) {
        set_status(obligation_id, ProofObligationStatus::Falsified, "matching Theory fact is falsified",
                   provenance_one(fact.id, "theory-fact", theory_.version, "falsified"));
        continue;
      }
      if (!has_nonempty_provenance(fact.provenance)) continue;
      const auto allowed_evidence = std::set<std::string>{"machine_executable_equality", "symbolic_derivation",
                                                            "formal_certificate", "type_checked", "numeric_certificate"};
      const bool explicit_evidence = has_evidence_type(fact, allowed_evidence);
      if (!explicit_evidence && fact.status != EpistemicStatus::StructuralDerivation &&
          fact.status != EpistemicStatus::SymbolicVerification && fact.status != EpistemicStatus::FormalVerification)
        continue;
      if (fact.kind == semantic::JudgmentKind::Equality &&
          semantic::rewrite_safety(fact, theory_, context_).safety != semantic::RewriteSafety::Allowed)
        continue;
      EvidenceLevel level = EvidenceLevel::TrustedFact;
      if (fact.status == EpistemicStatus::FormalVerification || has_evidence_type(fact, {"formal_certificate"}))
        level = EvidenceLevel::Formal;
      else if (fact.status == EpistemicStatus::SymbolicVerification || has_evidence_type(fact, {"symbolic_derivation"}))
        level = EvidenceLevel::Symbolic;
      else if (fact.status == EpistemicStatus::NumericalSupport)
        level = EvidenceLevel::NumericalSupportOnly;
      if (!evidence_satisfies(options_.required_evidence, level)) continue;
      const auto status = status_for_evidence(level, true);
      set_status(obligation_id, status, "exact semantic Theory fact matched with trusted evidence",
                 provenance_one(fact.id, "theory-fact", theory_.version, "exact semantic match"),
                 {evidence_one("trusted_fact", "layer18", theory_.version, fact.id)});
      add_evidence_node(obligation_id, fact.id, status,
                        provenance_one(fact.id, "theory-fact", theory_.version, "exact semantic match"));
    }
    if (unknown && obligation(obligation_id) && obligation(obligation_id)->status == ProofObligationStatus::Unresolved)
      set_status(obligation_id, ProofObligationStatus::BlockedUnknown,
                 "a possible Theory match has unknown semantic compatibility",
                 provenance_one(target.id, "layer18-fact-match", theory_.version, "unknown"));
  }

  void choose_plan_status() {
    if (!plan_.cycles.empty()) {
      plan_.status = ProofPlanStatus::Cyclic;
      plan_.status_reason = "one or more proof dependency cycles were retained as unresolved";
      return;
    }
    if (plan_.accounting.falsified != 0) {
      plan_.status = ProofPlanStatus::Falsified;
      plan_.status_reason = "at least one proof obligation is explicitly falsified";
      return;
    }
    if (plan_.accounting.contradicted != 0) {
      plan_.status = ProofPlanStatus::Contradicted;
      plan_.status_reason = "at least one proof obligation conflicts with the active context/regime";
      return;
    }
    if (plan_.accounting.unknown != 0) {
      plan_.status = ProofPlanStatus::BlockedUnknown;
      plan_.status_reason = "at least one proof prerequisite has UNKNOWN semantic status";
      return;
    }
    if (plan_.accounting.unsupported != 0) {
      plan_.status = ProofPlanStatus::Unsupported;
      plan_.status_reason = "a required rule, evidence type, or provenance contract is unsupported";
      return;
    }
    const bool all_roots = std::all_of(plan_.root_obligation_ids.begin(), plan_.root_obligation_ids.end(), [&](const auto& id) {
      const auto* root = obligation(id);
      if (!root) return false;
      if (root->status == ProofObligationStatus::NumericallySupported)
        return options_.required_evidence == EvidenceLevel::NumericalSupportOnly;
      return is_terminal_discharge(root->status) &&
             evidence_satisfies(options_.required_evidence, evidence_level_for_status(root->status));
    });
    if (plan_.accounting.open != 0 || !all_roots) {
      plan_.status = ProofPlanStatus::IncompleteOpenObligations;
      plan_.status_reason = "proof plan retains OPEN obligations at the requested evidence level";
      return;
    }
    plan_.status = ProofPlanStatus::CompleteAtRequiredLevel;
    plan_.status_reason = options_.required_evidence == EvidenceLevel::NumericalSupportOnly
                              ? "complete at explicitly requested numerical-support-only level; not a proof"
                              : "all obligations are discharged at the requested limited evidence level";
  }

  std::set<SemanticId> checked_semantics_;
};

void initialize_plan(ProofPlan& plan, const Judgment& target, const Theory& theory, const Context& context,
                     ProofPlanningOptions options, const reasoning::SolutionCandidate* candidate) {
  plan.target = normalized(target, context);
  plan.context = context;
  plan.regime = plan.target.regime;
  plan.required_evidence = options.required_evidence;
  plan.structural_candidate_id = candidate ? candidate->id : SemanticId{};
  plan.provenance = provenance_one(candidate ? candidate->id : plan.target.id,
                                   candidate ? "layer17-to-layer18" : "layer18-planner",
                                   theory.version,
                                   candidate ? "structural candidate converted to proof obligations"
                                             : "target-only proof plan");
}

void finalize_plan_id(ProofPlan& plan) {
  plan.refresh_id();
}

}  // namespace

const char* to_string(EvidenceLevel value) {
  switch (value) {
    case EvidenceLevel::Structural: return "STRUCTURAL";
    case EvidenceLevel::TrustedFact: return "TRUSTED_FACT";
    case EvidenceLevel::Symbolic: return "SYMBOLIC";
    case EvidenceLevel::Formal: return "FORMAL";
    case EvidenceLevel::NumericalSupportOnly: return "NUMERICAL_SUPPORT_ONLY";
  }
  return "UNKNOWN";
}

const char* to_string(ProofPlanStatus value) {
  switch (value) {
    case ProofPlanStatus::CompleteAtRequiredLevel: return "COMPLETE_AT_REQUIRED_LEVEL";
    case ProofPlanStatus::IncompleteOpenObligations: return "INCOMPLETE_OPEN_OBLIGATIONS";
    case ProofPlanStatus::BlockedUnknown: return "BLOCKED_UNKNOWN";
    case ProofPlanStatus::Falsified: return "FALSIFIED";
    case ProofPlanStatus::Contradicted: return "CONTRADICTED";
    case ProofPlanStatus::Cyclic: return "CYCLIC";
    case ProofPlanStatus::Unsupported: return "UNSUPPORTED";
    case ProofPlanStatus::InvalidDerivation: return "INVALID_DERIVATION";
  }
  return "UNSUPPORTED";
}

const char* to_string(ProofNodeKind value) {
  switch (value) {
    case ProofNodeKind::Obligation: return "OBLIGATION";
    case ProofNodeKind::RuleApplication: return "RULE_APPLICATION";
    case ProofNodeKind::Evidence: return "EVIDENCE";
  }
  return "OBLIGATION";
}

const char* to_string(ProofEdgeKind value) {
  switch (value) {
    case ProofEdgeKind::DependsOn: return "DEPENDS_ON";
    case ProofEdgeKind::DerivedBy: return "DERIVED_BY";
    case ProofEdgeKind::Alternative: return "ALTERNATIVE";
    case ProofEdgeKind::Supports: return "SUPPORTS";
  }
  return "DEPENDS_ON";
}

const char* to_string(ProofRuleKind value) {
  switch (value) {
    case ProofRuleKind::ExplicitRule: return "EXPLICIT_RULE";
    case ProofRuleKind::StructuralLineage: return "STRUCTURAL_LINEAGE";
    case ProofRuleKind::TypeCheck: return "TYPE_CHECK";
    case ProofRuleKind::RegimeCheck: return "REGIME_CHECK";
    case ProofRuleKind::SideCondition: return "SIDE_CONDITION";
    case ProofRuleKind::TrustedFact: return "TRUSTED_FACT";
  }
  return "EXPLICIT_RULE";
}

const char* to_string(CertificateStatus value) {
  switch (value) {
    case CertificateStatus::Unchecked: return "UNCHECKED";
    case CertificateStatus::Accepted: return "ACCEPTED";
    case CertificateStatus::Rejected: return "REJECTED";
    case CertificateStatus::Invalidated: return "INVALIDATED";
  }
  return "UNCHECKED";
}

void ProofCertificate::refresh_id() { id = semantic::deterministic_id("proof_certificate", canonical()); }
std::string ProofCertificate::canonical() const {
  return list("certificate", {obligation_id, backend, to_string(evidence_level), deterministic_payload,
                               to_string(status), provenance.canonical()});
}

void ProofRule::refresh_id() { id = semantic::deterministic_id("proof_rule", canonical()); }
std::string ProofRule::canonical() const {
  return list("proof_rule", {name, to_string(kind), pattern_context.canonical(), conclusion.canonical(),
                              canonical_judgments(premises), canonical_constraints(conditions), regime.canonical(),
                              provenance.canonical(), to_string(evidence_level), proof_safe ? "safe" : "unsafe"});
}

ProofRule proof_rule_from_goal_rule(const reasoning::GoalRule& source, bool proof_safe, ProofRuleKind kind) {
  ProofRule result;
  result.id = source.id;
  result.name = source.name;
  result.kind = kind;
  result.pattern_context = source.pattern_context;
  result.conclusion = source.conclusion;
  result.premises = source.premises;
  result.conditions = source.conditions;
  result.regime = source.regime;
  result.provenance = source.provenance;
  result.evidence_level = EvidenceLevel::Structural;
  result.proof_safe = proof_safe;
  return result;
}

void ProofPlanNode::refresh_id() {
  id = semantic::deterministic_id("proof_plan_node",
                                 list("node_identity", {to_string(kind), obligation_id, rule_id, certificate_id,
                                                         context_id, regime_id, label}));
}
std::string ProofPlanNode::canonical() const {
  return list("proof_plan_node", {id, to_string(kind), obligation_id, rule_id, certificate_id, context_id, regime_id,
                                  semantic::to_string(status), label, provenance.canonical()});
}

void ProofPlanEdge::refresh_id() {
  id = semantic::deterministic_id("proof_plan_edge",
                                 list("edge_identity", {source_id, target_id, to_string(kind), branch_id,
                                                         context_id, regime_id}));
}
std::string ProofPlanEdge::canonical() const {
  return list("proof_plan_edge", {id, source_id, target_id, to_string(kind), branch_id, context_id, regime_id,
                                   provenance.canonical()});
}

bool ProofPlanAccounting::consistent() const {
  return generated_obligations == unique_obligations + duplicate_obligations &&
         unique_obligations == automatically_discharged + open + unknown + falsified + contradicted + cyclic + unsupported +
                                      numerically_supported;
}

std::string ProofPlanAccounting::canonical() const {
  return list("proof_accounting", {std::to_string(generated_obligations), std::to_string(unique_obligations),
                                    std::to_string(duplicate_obligations), std::to_string(automatically_discharged),
                                    std::to_string(open), std::to_string(unknown), std::to_string(falsified),
                                    std::to_string(contradicted), std::to_string(cyclic), std::to_string(unsupported),
                                    std::to_string(numerically_supported)});
}

void ProofPlan::refresh_id() { id = semantic::deterministic_id("proof_plan", identity_canonical()); }
std::string ProofPlan::identity_canonical() const {
  std::vector<std::string> obligation_values;
  for (const auto& obligation : obligations)
    obligation_values.push_back(list("obligation_identity", {obligation.id, obligation.label, obligation.target.canonical()}));
  std::vector<std::string> node_values;
  for (const auto& node : nodes)
    node_values.push_back(list("node_identity", {to_string(node.kind), node.obligation_id, node.rule_id,
                                                 node.certificate_id, node.context_id, node.regime_id, node.label}));
  std::vector<std::string> edge_values;
  for (const auto& edge : edges)
    edge_values.push_back(list("edge_identity", {edge.source_id, edge.target_id, to_string(edge.kind), edge.branch_id,
                                                 edge.context_id, edge.regime_id}));
  return list("proof_plan_identity", {target.canonical(), structural_candidate_id, context.canonical(), regime.canonical(),
                                       scope.canonical(), to_string(required_evidence), provenance.canonical(),
                                       list("obligations", obligation_values), list("nodes", node_values),
                                       list("edges", edge_values), list("roots", root_obligation_ids)});
}
std::string ProofPlan::canonical() const {
  std::vector<std::string> obligations_value;
  for (const auto& obligation : obligations)
    obligations_value.push_back(obligation.canonical() + obligation.required_evidence + semantic::to_string(obligation.status));
  std::vector<std::string> node_values;
  for (const auto& node : nodes) node_values.push_back(node.canonical());
  std::vector<std::string> edge_values;
  for (const auto& edge : edges) edge_values.push_back(edge.canonical());
  std::vector<std::string> cycle_values;
  for (const auto& cycle : cycles) cycle_values.push_back(list("cycle", cycle));
  return list("proof_plan", {id, target.canonical(), structural_candidate_id, context.canonical(), regime.canonical(),
                              scope.canonical(), to_string(required_evidence), to_string(status), status_reason,
                              list("roots", root_obligation_ids), list("obligations", obligations_value),
                              list("nodes", node_values), list("edges", edge_values), list("cycles", cycle_values),
                              accounting.canonical(), list("unresolved", unresolved_obligation_ids)});
}

ProofPlan ProofPlanner::plan(const reasoning::GoalSearchResult& search, std::size_t solution_index,
                             const Theory& theory, const Context& context,
                             const std::vector<ProofRule>& rules,
                             const std::vector<ProofCertificate>& certificates,
                             ProofPlanningOptions options) const {
  ProofPlan plan;
  if (solution_index >= search.solutions.size()) {
    initialize_plan(plan, search.target, theory, context, options, nullptr);
    plan.status = ProofPlanStatus::InvalidDerivation;
    plan.status_reason = "requested Layer-17 solution index does not exist";
    plan.refresh_id();
    return plan;
  }
  const auto& candidate = search.solutions[solution_index];
  initialize_plan(plan, candidate.target, theory, context, options, &candidate);
  plan.scope = candidate.scope;
  PlannerImpl impl(plan, theory, context, rules, certificates, options);
  impl.set_structural_candidate(&candidate);
  impl.add_structural_snapshot(search, candidate);
  impl.initialize_candidate_root(&candidate);
  impl.add_lineage_obligations(search, candidate);
  impl.add_candidate_conditions(candidate);
  impl.mark_missing_lineage_rules();
  impl.finalize();
  finalize_plan_id(plan);
  return plan;
}

ProofPlan ProofPlanner::plan(const reasoning::SolutionCandidate& candidate, const Theory& theory, const Context& context,
                             const std::vector<ProofRule>& rules, const std::vector<ProofCertificate>& certificates,
                             ProofPlanningOptions options) const {
  ProofPlan plan;
  initialize_plan(plan, candidate.target, theory, context, options, &candidate);
  plan.scope = candidate.scope;
  PlannerImpl impl(plan, theory, context, rules, certificates, options);
  impl.set_structural_candidate(&candidate);
  impl.initialize_candidate_root(&candidate);
  impl.add_candidate_conditions(candidate);
  impl.finalize();
  finalize_plan_id(plan);
  return plan;
}

ProofPlan ProofPlanner::plan(const Judgment& target, const Theory& theory, const Context& context,
                             const std::vector<ProofRule>& rules, const std::vector<ProofCertificate>& certificates,
                             ProofPlanningOptions options) const {
  ProofPlan plan;
  initialize_plan(plan, target, theory, context, options, nullptr);
  PlannerImpl impl(plan, theory, context, rules, certificates, options);
  impl.initialize_direct_root(plan.target);
  impl.finalize();
  finalize_plan_id(plan);
  return plan;
}

ProofPlan ProofPlanner::replay(const ProofPlan& previous, const Theory& theory, const Context& context,
                               const std::vector<ProofRule>& rules, const std::vector<ProofCertificate>& certificates,
                               ProofPlanningOptions options) const {
  ProofPlan replayed = previous;
  replayed.context = context;
  replayed.regime = replayed.target.regime;
  replayed.required_evidence = options.required_evidence;
  PlannerImpl impl(replayed, theory, context, rules, certificates, options);
  impl.replay_statuses(previous);
  impl.finalize();
  finalize_plan_id(replayed);
  return replayed;
}

namespace {

Context benchmark_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

Theory benchmark_theory() {
  Theory theory;
  theory.version = "layer18-controlled-theory-v1";
  theory.provenance = "layer18-benchmark-fixture";
  theory.add_operator({"op.A", "A", semantic::TypeRef::named("Scalar"), semantic::TypeRef::named("Scalar"), {}, {},
                       "layer18-fixture"});
  theory.add_operator({"op.B", "B", semantic::TypeRef::named("Scalar"), semantic::TypeRef::named("Scalar"), {}, {},
                       "layer18-fixture"});
  theory.add_operator({"op.C", "C", semantic::TypeRef::named("Scalar"), semantic::TypeRef::named("Scalar"), {}, {},
                       "layer18-fixture"});
  theory.refresh_id();
  return theory;
}

Judgment benchmark_definedness(const Context& context, const std::string& operator_id) {
  Judgment target;
  target.kind = semantic::JudgmentKind::Definedness;
  target.context_id = context.id;
  target.regime = context.active_regime;
  target.operands = {semantic::Expression::operator_reference(operator_id)};
  target.status = EpistemicStatus::StructuralCandidate;
  target.refresh_id();
  return target;
}

reasoning::GoalSearchScope benchmark_scope(const Theory& theory, const Context& context) {
  reasoning::GoalSearchScope scope;
  scope.quotient_scope.theory_id = theory.id;
  scope.quotient_scope.theory_version = theory.version;
  scope.quotient_scope.grammar_id = "layer18-proof-fixture-grammar-v1";
  scope.quotient_scope.allowed_construction_kinds = {"atom", "composition"};
  scope.quotient_scope.max_depth = 1;
  scope.quotient_scope.equivalence_theory_id = "layer16-proof-fixture-equivalence-v1";
  scope.quotient_scope.context_id = context.id;
  scope.quotient_scope.regime = context.active_regime;
  scope.quotient_scope.deterministic_seed = 18;
  scope.forward_grammar_id = "layer18-proof-fixture-grammar-v1";
  scope.backward_rule_set_id = "layer18-proof-fixture-rules-v1";
  scope.max_forward_depth = 1;
  scope.max_backward_depth = 2;
  scope.refresh_id();
  return scope;
}

reasoning::SolutionCandidate benchmark_candidate(const Theory& theory, const Context& context,
                                                 const Judgment& target, const std::string& tag) {
  reasoning::SolutionCandidate candidate;
  candidate.target = target;
  candidate.context_id = context.id;
  candidate.regime = context.active_regime;
  candidate.scope = benchmark_scope(theory, context);
  candidate.status = EpistemicStatus::StructuralCandidate;
  candidate.complete = true;
  candidate.forward_lineage = {tag + ".forward"};
  candidate.backward_lineage = {tag + ".backward"};
  candidate.refresh_id();
  return candidate;
}

ProofRule benchmark_rule(const Context& context, const std::string& conclusion_id, const std::string& premise_id,
                         bool proof_safe = true) {
  ProofRule rule;
  rule.name = "explicit prerequisite " + conclusion_id + " from " + premise_id;
  rule.pattern_context = context;
  rule.conclusion = benchmark_definedness(context, conclusion_id);
  rule.premises = {benchmark_definedness(context, premise_id)};
  rule.regime = context.active_regime;
  rule.provenance = provenance_one("layer18.rule." + conclusion_id, "layer18-proof-rule", "layer18-controlled-theory-v1",
                                   "explicit AND prerequisite fixture");
  rule.proof_safe = proof_safe;
  rule.refresh_id();
  return rule;
}

ProofRule shared_rule(const Context& context, const std::string& name, const std::string& premise_id) {
  auto rule = benchmark_rule(context, "op.C", premise_id);
  rule.name = name;
  rule.refresh_id();
  return rule;
}

ProofPlan plan_direct(const Judgment& target, const Theory& theory, const Context& context,
                      const std::vector<ProofRule>& rules = {}, EvidenceLevel level = EvidenceLevel::Structural) {
  ProofPlanningOptions options;
  options.required_evidence = level;
  return ProofPlanner{}.plan(target, theory, context, rules, {}, options);
}

ProofPlan plan_candidate(const reasoning::SolutionCandidate& candidate, const Theory& theory, const Context& context,
                         const std::vector<ProofRule>& rules = {}, EvidenceLevel level = EvidenceLevel::Structural) {
  ProofPlanningOptions options;
  options.required_evidence = level;
  return ProofPlanner{}.plan(candidate, theory, context, rules, {}, options);
}

Layer18BenchmarkOutcome outcome(std::string id, std::string category, ProofPlan plan) {
  return {std::move(id), std::move(category), std::move(plan)};
}

ProofPlan trusted_fact_case() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.A");
  target.status = EpistemicStatus::Unresolved;
  target.refresh_id();
  auto fact = target;
  fact.status = EpistemicStatus::StructuralDerivation;
  fact.provenance = provenance_one("fixture.fact.op.A", "layer18-trusted-fact", theory.version, "typed operator declaration");
  fact.evidence = {evidence_one("type_checked", "layer18-fixture", theory.version, "valid")};
  theory.add_fact(fact);
  theory.refresh_id();
  return plan_direct(target, theory, context, {}, EvidenceLevel::TrustedFact);
}

ProofPlan open_obligation_case() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.B");
  auto candidate = benchmark_candidate(theory, context, target, "open");
  return plan_candidate(candidate, theory, context, {benchmark_rule(context, "op.B", "op.A")});
}

ProofPlan unknown_regime_case() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  context.active_regime.constraints = {{semantic::ConstraintKind::Regularity, semantic::ConstraintRelation::AtLeast,
                                        "regularity", "1"}};
  context.active_regime.refresh_id();
  context.refresh_id();
  auto target = benchmark_definedness(context, "op.A");
  target.regime.constraints = {{semantic::ConstraintKind::Regularity, semantic::ConstraintRelation::AtMost,
                                "regularity", "1"}};
  target.regime.refresh_id();
  target.refresh_id();
  return plan_direct(target, theory, context);
}

ProofPlan falsified_case() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.A");
  target.status = EpistemicStatus::Falsified;
  target.refresh_id();
  return plan_direct(target, theory, context);
}

ProofPlan contradicted_case() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  context.active_regime.constraints = {{semantic::ConstraintKind::Geometry, semantic::ConstraintRelation::Equals,
                                        "geometry", "euclidean"}};
  context.active_regime.refresh_id();
  context.refresh_id();
  auto target = benchmark_definedness(context, "op.A");
  target.regime.constraints = {{semantic::ConstraintKind::Geometry, semantic::ConstraintRelation::Equals,
                                "geometry", "curved"}};
  target.regime.refresh_id();
  target.refresh_id();
  return plan_direct(target, theory, context);
}

ProofPlan shared_dag_case() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.C");
  auto fact = benchmark_definedness(context, "op.A");
  fact.status = EpistemicStatus::StructuralDerivation;
  fact.provenance = provenance_one("fixture.shared.op.A", "layer18-trusted-fact", theory.version, "shared premise");
  fact.evidence = {evidence_one("type_checked", "layer18-fixture", theory.version, "valid")};
  theory.add_fact(fact);
  theory.refresh_id();
  return plan_direct(target, theory, context,
                     {shared_rule(context, "branch-one", "op.A"), shared_rule(context, "branch-two", "op.A")},
                     EvidenceLevel::Structural);
}

ProofPlan cyclic_case() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.A");
  return plan_direct(target, theory, context,
                     {benchmark_rule(context, "op.A", "op.B"), benchmark_rule(context, "op.B", "op.A")});
}

ProofPlan indexed_case() {
  Theory theory;
  theory.version = "layer18-indexed-theory-v1";
  theory.provenance = "layer18-indexed-fixture";
  semantic::OperatorDeclaration derivative;
  derivative.id = "op.d";
  derivative.name = "d";
  derivative.index_parameters = {"k"};
  derivative.domain = semantic::TypeRef::indexed("Form", {semantic::TypeArgument::index("k")});
  derivative.codomain = semantic::TypeRef::indexed("Form", {semantic::TypeArgument::index("k", 1)});
  derivative.provenance = "layer18-indexed-fixture";
  theory.add_operator(derivative);
  theory.refresh_id();
  auto context = benchmark_context();
  const auto d_k = semantic::Expression::indexed_operator_reference("op.d", {semantic::IndexTerm::variable("k")});
  const auto d_k1 = semantic::Expression::indexed_operator_reference("op.d", {semantic::IndexTerm::variable("k", 1)});
  auto target = Judgment{};
  target.kind = semantic::JudgmentKind::Nilpotence;
  target.context_id = context.id;
  target.regime = context.active_regime;
  target.operands = {semantic::Expression::composition(d_k1, d_k),
                     semantic::Expression::zero(semantic::TypeRef::indexed("Form", {semantic::TypeArgument::index("k", 1)}))};
  target.relation_name = "d_(k+1) o d_k = 0";
  target.status = EpistemicStatus::Unresolved;
  target.refresh_id();
  auto fact = target;
  fact.status = EpistemicStatus::StructuralDerivation;
  fact.provenance = provenance_one("fixture.nilpotence.d", "layer18-trusted-fact", theory.version,
                                   "indexed nilpotence judgment with adjacent index relation");
  fact.evidence = {evidence_one("symbolic_derivation", "layer18-fixture", theory.version, "indexed judgment recorded")};
  theory.add_fact(fact);
  theory.refresh_id();
  return plan_direct(target, theory, context, {}, EvidenceLevel::Symbolic);
}

ProofPlan analogy_control() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.A");
  auto analogy = target;
  analogy.kind = semantic::JudgmentKind::Analogy;
  analogy.operands = {semantic::Expression::operator_reference("op.A"), semantic::Expression::operator_reference("op.B")};
  analogy.status = EpistemicStatus::Observation;
  analogy.refresh_id();
  theory.add_fact(analogy);
  theory.refresh_id();
  return plan_direct(target, theory, context);
}

ProofPlan unknown_side_condition_control() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.A");
  target.side_conditions = {{semantic::ConstraintKind::Geometry, semantic::ConstraintRelation::Equals,
                             "geometry", "euclidean"}};
  target.refresh_id();
  return plan_direct(target, theory, context);
}

ProofPlan display_name_control() {
  auto theory = benchmark_theory();
  theory.operators.at("op.B").name = "A";
  theory.refresh_id();
  auto context = benchmark_context();
  auto fact = benchmark_definedness(context, "op.B");
  fact.status = EpistemicStatus::StructuralDerivation;
  fact.provenance = provenance_one("fixture.display-name.op.B", "layer18-trusted-fact", theory.version, "different semantic id");
  fact.evidence = {evidence_one("type_checked", "layer18-fixture", theory.version, "valid")};
  theory.add_fact(fact);
  theory.refresh_id();
  return plan_direct(benchmark_definedness(context, "op.A"), theory, context);
}

ProofPlan numeric_formal_control() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.A");
  auto fact = target;
  fact.status = EpistemicStatus::NumericalSupport;
  fact.provenance = provenance_one("fixture.numeric", "numeric-fixture", theory.version, "not mathematical proof");
  fact.evidence = {evidence_one("numeric_certificate", "numeric-fixture", theory.version, "supported")};
  theory.add_fact(fact);
  theory.refresh_id();
  ProofPlanningOptions options;
  options.required_evidence = EvidenceLevel::Formal;
  return ProofPlanner{}.plan(target, theory, context, {}, {}, options);
}

ProofPlan numeric_support_only_case() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.A");
  auto fact = target;
  fact.status = EpistemicStatus::NumericalSupport;
  fact.provenance = provenance_one("fixture.numeric.support", "numeric-fixture", theory.version,
                                   "represented support only; no numerical experiment is run by Layer 18");
  fact.evidence = {evidence_one("numeric_certificate", "fixture", theory.version, "support-only")};
  theory.add_fact(fact);
  theory.refresh_id();
  ProofPlanningOptions options;
  options.required_evidence = EvidenceLevel::NumericalSupportOnly;
  return ProofPlanner{}.plan(target, theory, context, {}, {}, options);
}

ProofPlan missing_provenance_control() {
  auto theory = benchmark_theory();
  auto context = benchmark_context();
  auto target = benchmark_definedness(context, "op.A");
  auto fact = target;
  fact.status = EpistemicStatus::StructuralDerivation;
  fact.evidence = {evidence_one("type_checked", "fixture", theory.version, "valid")};
  theory.add_fact(fact);
  theory.refresh_id();
  return plan_direct(target, theory, context, {}, EvidenceLevel::TrustedFact);
}

}  // namespace

Layer18BenchmarkReport run_layer18_benchmarks() {
  Layer18BenchmarkReport report;
  report.outcomes.push_back(outcome("trusted-fact", "fully_discharged_trusted_fact", trusted_fact_case()));
  report.outcomes.push_back(outcome("open-premise", "open_obligation", open_obligation_case()));
  report.outcomes.push_back(outcome("unknown-regime", "unknown_regime", unknown_regime_case()));
  report.outcomes.push_back(outcome("falsified-target", "falsified", falsified_case()));
  report.outcomes.push_back(outcome("contradicted-regime", "contradicted", contradicted_case()));
  report.outcomes.push_back(outcome("shared-dag", "shared_obligation_dag", shared_dag_case()));
  report.outcomes.push_back(outcome("cyclic", "cyclic_dependency", cyclic_case()));
  report.indexed_plan = indexed_case();
  report.negative_controls.push_back(outcome("analogy-not-equality", "weak_relation_cannot_discharge", analogy_control()));
  report.negative_controls.push_back(outcome("unknown-side-condition", "unknown_cannot_discharge", unknown_side_condition_control()));
  report.negative_controls.push_back(outcome("display-name-only", "semantic_id_required", display_name_control()));
  report.negative_controls.push_back(outcome("numeric-vs-formal", "numeric_support_not_formal", numeric_formal_control()));
  report.negative_controls.push_back(outcome("missing-provenance", "provenance_required", missing_provenance_control()));
  report.negative_controls.push_back(outcome("numeric-support-only", "numeric_status_is_not_proof", numeric_support_only_case()));

  const auto layer17 = reasoning::run_layer17_benchmarks();
  const auto composition_case = reasoning::layer17_positive_cases()[0];
  std::vector<ProofRule> composition_proof_rules;
  for (const auto& rule : composition_case.problem.rules)
    composition_proof_rules.push_back(proof_rule_from_goal_rule(rule, true, ProofRuleKind::StructuralLineage));
  report.outcomes.push_back(outcome(
      "layer17-composition", "real_layer17_structural_candidate",
      ProofPlanner{}.plan(layer17.positive[0].result, 0, composition_case.problem.theory,
                          composition_case.problem.context, composition_proof_rules)));
  const auto multiple_case = reasoning::layer17_positive_cases()[4];
  for (std::size_t solution_index = 0; solution_index < layer17.positive[4].result.solutions.size(); ++solution_index) {
    const auto& solution = layer17.positive[4].result.solutions[solution_index];
    report.outcomes.push_back(outcome("layer17-alternative-" + solution.id, "separate_structural_solution",
                                      ProofPlanner{}.plan(layer17.positive[4].result, solution_index,
                                                          multiple_case.problem.theory,
                                                          multiple_case.problem.context)));
  }
  report.indexed_plan.refresh_id();
  std::vector<std::string> digests;
  for (const auto& item : report.outcomes) digests.push_back(item.plan.canonical());
  digests.push_back(report.indexed_plan.canonical());
  for (const auto& item : report.negative_controls) digests.push_back(item.plan.canonical());
  report.deterministic_digest = semantic::deterministic_id("layer18_benchmark_digest", list("plans", digests));
  return report;
}

std::string export_text(const ProofPlan& plan) {
  std::ostringstream out;
  out << "Status: " << to_string(plan.status) << "\n"
      << "Status reason: " << plan.status_reason << "\n"
      << "Plan ID: " << plan.id << "\n"
      << "Target: " << plan.target.id << "\n"
      << "Required evidence: " << to_string(plan.required_evidence) << "\n"
      << "Structural candidate: " << (plan.structural_candidate_id.empty() ? "none" : plan.structural_candidate_id) << "\n"
      << "Obligations generated/unique/duplicates: " << plan.accounting.generated_obligations << "/"
      << plan.accounting.unique_obligations << "/" << plan.accounting.duplicate_obligations << "\n"
      << "Discharged/open/unknown/falsified/contradicted/cyclic/unsupported/numeric: "
      << plan.accounting.automatically_discharged << "/" << plan.accounting.open << "/" << plan.accounting.unknown << "/"
      << plan.accounting.falsified << "/" << plan.accounting.contradicted << "/" << plan.accounting.cyclic << "/"
      << plan.accounting.unsupported << "/" << plan.accounting.numerically_supported << "\n"
      << "DAG nodes/edges: " << plan.nodes.size() << "/" << plan.edges.size() << "\n"
      << "Cycles: " << plan.cycles.size() << "\n"
      << "Accounting consistent: " << (plan.accounting_consistent() ? "yes" : "no") << "\n";
  for (const auto& obligation : plan.obligations)
    out << "Obligation " << obligation.id << " status=" << semantic::to_string(obligation.status)
        << " label=" << obligation.label << " reason=" << obligation.reason << "\n";
  return out.str();
}

std::string export_json(const ProofPlan& plan) {
  std::ostringstream out;
  out << "{\"id\":\"" << plan.id << "\",\"status\":\"" << to_string(plan.status)
      << "\",\"status_reason\":\"" << plan.status_reason << "\",\"required_evidence\":\""
      << to_string(plan.required_evidence) << "\",\"candidate\":\"" << plan.structural_candidate_id
      << "\",\"generated\":" << plan.accounting.generated_obligations << ",\"unique\":"
      << plan.accounting.unique_obligations << ",\"duplicates\":" << plan.accounting.duplicate_obligations
      << ",\"discharged\":" << plan.accounting.automatically_discharged << ",\"open\":" << plan.accounting.open
      << ",\"unknown\":" << plan.accounting.unknown << ",\"falsified\":" << plan.accounting.falsified
      << ",\"contradicted\":" << plan.accounting.contradicted << ",\"cyclic\":" << plan.accounting.cyclic
      << ",\"unsupported\":" << plan.accounting.unsupported << ",\"numeric\":"
      << plan.accounting.numerically_supported << ",\"nodes\":" << plan.nodes.size() << ",\"edges\":"
      << plan.edges.size() << ",\"cycles\":" << plan.cycles.size() << ",\"accounting_consistent\":"
      << (plan.accounting_consistent() ? "true" : "false") << "}";
  return out.str();
}

std::string export_text(const Layer18BenchmarkReport& report) {
  std::ostringstream out;
  out << "Layer 18 proof-planning benchmarks:\n";
  for (const auto& item : report.outcomes)
    out << item.id << " category=" << item.category << "\n" << export_text(item.plan);
  out << "Indexed benchmark:\n" << export_text(report.indexed_plan);
  out << "Layer 18 negative controls:\n";
  for (const auto& item : report.negative_controls)
    out << item.id << " category=" << item.category << "\n" << export_text(item.plan);
  out << "Deterministic digest: " << report.deterministic_digest << "\n";
  return out.str();
}

std::string export_json(const Layer18BenchmarkReport& report) {
  std::ostringstream out;
  out << "{\"outcomes\":[";
  for (std::size_t index = 0; index < report.outcomes.size(); ++index) {
    if (index) out << ',';
    out << "{\"id\":\"" << report.outcomes[index].id << "\",\"category\":\""
        << report.outcomes[index].category << "\",\"plan\":" << export_json(report.outcomes[index].plan) << "}";
  }
  out << "],\"indexed\":" << export_json(report.indexed_plan) << ",\"negative_controls\":[";
  for (std::size_t index = 0; index < report.negative_controls.size(); ++index) {
    if (index) out << ',';
    out << "{\"id\":\"" << report.negative_controls[index].id << "\",\"category\":\""
        << report.negative_controls[index].category << "\",\"plan\":"
        << export_json(report.negative_controls[index].plan) << "}";
  }
  out << "],\"digest\":\"" << report.deterministic_digest << "\"}";
  return out.str();
}

}  // namespace opforge::proof
