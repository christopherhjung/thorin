#ifndef THORIN_GLUE
#error "define THORIN_GLUE before including this file"
#endif

#ifndef THORIN_AIR_NODE
#error "define THORIN_AIR_NODE before including this file"
#endif

#ifndef THORIN_PRIMTYPE
#error "define THORIN_PRIMTYPE before including this file"
#endif

#ifndef THORIN_ARITHOP
#error "define THORIN_ARITHOP before including this file"
#endif

#ifndef THORIN_CMP
#error "define THORIN_CMP before including this file"
#endif

#ifndef THORIN_CONVOP
#error "define THORIN_CONVOP before including this file"
#endif

#include "thorin/tables/nodetable.h"
    THORIN_GLUE(Node, PrimType_s)
#define THORIN_S_TYPE(T) THORIN_PRIMTYPE(T)
#include "thorin/tables/primtypetable.h"
    THORIN_GLUE(PrimType_s, PrimType_u)
#define THORIN_U_TYPE(T) THORIN_PRIMTYPE(T)
#include "thorin/tables/primtypetable.h"
    THORIN_GLUE(PrimType_u, PrimType_qs)
#define THORIN_QS_TYPE(T) THORIN_PRIMTYPE(T)
#include "thorin/tables/primtypetable.h"
    THORIN_GLUE(PrimType_qs, PrimType_qu)
#define THORIN_QU_TYPE(T) THORIN_PRIMTYPE(T)
#include "thorin/tables/primtypetable.h"
    THORIN_GLUE(PrimType_qu, PrimType_f)
#define THORIN_F_TYPE(T) THORIN_PRIMTYPE(T)
#include "thorin/tables/primtypetable.h"
    THORIN_GLUE(PrimType_f, PrimType_qf)
#define THORIN_QF_TYPE(T) THORIN_PRIMTYPE(T)
#include "thorin/tables/primtypetable.h"
    THORIN_GLUE(PrimType_qf, ArithOp)
#include "thorin/tables/arithoptable.h"
    THORIN_GLUE(ArithOp, Cmp)
#include "thorin/tables/cmptable.h"
    THORIN_GLUE(Cmp, ConvOp)
#include "thorin/tables/convoptable.h"

#undef THORIN_GLUE
#undef THORIN_AIR_NODE
#undef THORIN_PRIMTYPE
#undef THORIN_ARITHOP
#undef THORIN_CMP
#undef THORIN_CONVOP
