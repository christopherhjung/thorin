#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/copy_prop.h"
#include "thorin/pass/fp/dce.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/ssa_constr.h"
#include "thorin/pass/rw/auto_diff.h"
#include "thorin/pass/rw/bound_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

// old stuff
#include "thorin/transform/cleanup_world.h"
#include "thorin/transform/partial_evaluation.h"


namespace thorin {

void optimize(World& world) {

    world.set(LogLevel::Debug);

    PassMan opt(world);
    opt.add<AutoDiff>();
    opt.run();
    printf("Finished Opti1\n");


    PassMan opt2(world);
    opt2.add<PartialEval>();
    opt2.add<BetaRed>();
    auto er = opt2.add<EtaRed>();
    auto ee = opt2.add<EtaExp>(er);
    opt2.add<SSAConstr>(ee);
    opt2.run();
    printf("Finished Opti2\n");


        cleanup_world(world);
    partial_evaluation(world, true);
        cleanup_world(world);

    printf("Finished Cleanup\n");

    PassMan codgen_prepare(world);
    codgen_prepare.add<RetWrap>();
    codgen_prepare.run();
}

}
