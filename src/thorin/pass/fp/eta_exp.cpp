#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"

namespace thorin {

const Proxy* EtaExp::proxy(Lam* lam) {
    return FPPass<EtaExp, Lam>::proxy(lam->type(), {lam}, 0);
}

Lam* EtaExp::new2old(Lam* new_lam) {
    if (auto old_lam = new2old_.lookup(new_lam)) {
        auto root = new2old(*old_lam); // path compression
        assert(root != new_lam);
        new2old_[new_lam] = root;
        return root;
    }

    return new_lam;
}

const Def* EtaExp::rewrite(const Def* def) {
    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        if (auto lam = def->op(i)->isa_nom<Lam>(); lam && lam->is_set()) {
            if (!isa_callee(def, i) && expand_.contains(lam)) {
                auto [j, ins] = def2exp_.emplace(def, nullptr);
                if (ins) {
                    auto wrap = eta_wrap(lam);
                    auto new_def = def->refine(i, wrap);
                    wrap2subst_[wrap] = std::pair(lam, new_def);
                    j->second = new_def;
                    world().DLOG("eta-expansion '{}' -> '{}' using '{}'", def, j->second, wrap);
                }
                return j->second;
            }

            if (auto subst = wrap2subst_.lookup(lam)) {
                if (auto [orig, subst_def] = *subst; def != subst_def) return reconvert(def);
            }
        }
    }

    return def;
}

/// If a wrapper is somehow reinstantiated again in a different expression, redo eta-conversion.
/// E.g., say we have <code>(a, f, g)</code> and eta-exand to <code>(a, eta_f, eta_g)</code>.
/// But due to beta-reduction we now also have (b, eta_f, eta_g) which renders eta_f and eta_g not unique anymore.
/// So, we build <code>(b, eta_f', eta_g')</code>.
/// Likewise, we might end up with a call <code>eta_f (a, b, c)</code> that we have to eta-reduce again to
/// <code>f (a, b, c)</code>
const Def* EtaExp::reconvert(const Def* def) {
    std::vector<std::pair<Lam*, Lam*>> refinements;
    DefArray new_ops(def->num_ops());

    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        if (auto lam = def->op(i)->isa_nom<Lam>()) {
            if (auto subst = wrap2subst_.lookup(lam)) {
                auto [orig, subst_def] = *subst;
                assert(lam->body()->isa<App>() && lam->body()->as<App>()->callee() == orig);
                if (isa_callee(def, i)) {
                    new_ops[i] = orig;
                } else {
                    auto wrap = eta_wrap(orig);
                    refinements.emplace_back(wrap, orig);
                    new_ops[i] = wrap;
                }
                continue;
            }
        }

        new_ops[i] = def->op(i);
    }

    auto new_def = def->rebuild(world(), def->type(), new_ops, def->dbg());

    for (auto [wrap, lam] : refinements)
        wrap2subst_[wrap] = std::pair(lam, new_def);

    return def2exp_[def] = new_def;
}

Lam* EtaExp::eta_wrap(Lam* lam) {
    auto wrap = lam->stub(world(), lam->type(), lam->dbg());
    wrap->set_name(std::string("eta_") + lam->debug().name);
    wrap->app(lam, wrap->var());
    if (eta_red_) eta_red_->mark_irreducible(wrap);
    return wrap;
}

undo_t EtaExp::analyze(const Proxy* proxy) {
    auto lam = proxy->op(0)->as_nom<Lam>();
    if (expand_.emplace(lam).second)
        return undo_visit(lam);
    return No_Undo;
}

undo_t EtaExp::analyze(const Def* def) {
    auto undo = No_Undo;
    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        if (auto lam = def->op(i)->isa_nom<Lam>(); lam && lam->is_set()) {
            lam = new2old(lam);
            if (expand_.contains(lam)) continue;

            if (isa_callee(def, i)) {
                auto [_, l] = *data().emplace(lam, Lattice::Callee).first;
                if (l == Lattice::Non_Callee_1) {
                    world().DLOG("Callee: Callee -> Expand: '{}'", lam);
                    expand_.emplace(lam);
                    undo = std::min(undo, undo_visit(lam));
                } else {
                    world().DLOG("Callee: Bot/Callee -> Callee: '{}'", lam);
                }
            } else {
                auto [it, first] = data().emplace(lam, Lattice::Non_Callee_1);

                if (first) {
                    world().DLOG("Non_Callee: Bot -> Non_Callee_1: '{}'", lam);
                } else {
                    world().DLOG("Non_Callee: {} -> Expand: '{}'", lattice2str(it->second), lam);
                    expand_.emplace(lam);
                    undo = std::min(undo, undo_visit(lam));
                }
            }
        }
    }

    return undo;
}

}
