#include "thorin/opt/optimizer.h"

#include "thorin/analyses/scope.h"
#include "thorin/opt/inliner.h"

namespace thorin {

void Optimizer::run() {
    auto externals = world().externals();
    for (auto lam : externals)
        enqueue(lam);

    while (!nominals_.empty()) {
        auto def = pop(nominals_);

        for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
            def->set(i, rewrite(def->op(i)));
            analyze(def->op(i));
        }

    }
}

void Optimizer::enqueue(Def* old_def) {
    if (old2new_.contains(old_def)) return;

    auto new_def = old_def;
    /* TODO
    for (auto&& opt : opts_)
        new_def = opt->visit(new_def);
    */
    old2new_[old_def] = new_def;
    nominals_.push(new_def);
}

const Def* Optimizer::rewrite(const Def* old_def) {
    if (old_def->isa_nominal()) return old_def;
    if (auto new_def = old2new_.lookup(old_def)) return *new_def;

    auto new_type = rewrite(old_def->type());

    bool rebuild = false;
    Array<const Def*> new_ops(old_def->num_ops(), [&](size_t i) {
        auto old_op = old_def->op(i);
        auto new_op = rewrite(old_op);
        rebuild |= old_op != new_op;
        return new_op;
    });

    auto new_def = rebuild ? old_def->rebuild(world(), new_type, new_ops) : old_def;

    for (auto&& opt : opts_)
        new_def = opt->rewrite(new_def);

    return old2new_[old_def] = new_def;
}

void Optimizer::analyze(const Def* def) {
    if (auto nominal = def->isa_nominal()) return enqueue(nominal);
    if (!analyzed_.emplace(def).second) return;

    for (auto op : def->ops())
        analyze(op);

    for (auto&& opt : opts_)
        opt->analyze(def);
}

Optimizer std_optimizer(World& world) {
    Optimizer result(world);
    result.create<Inliner>();
    return result;
}

}
