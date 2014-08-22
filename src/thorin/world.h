#ifndef THORIN_WORLD_H
#define THORIN_WORLD_H

#include <cassert>
#include <functional>
#include <initializer_list>
#include <queue>
#include <string>

#include "thorin/enums.h"
#include "thorin/lambda.h"
#include "thorin/primop.h"
#include "thorin/type.h"
#include "thorin/util/hash.h"

namespace thorin {

class Any;
class Bottom;
class DefNode;
class Enter;
class LEA;
class Lambda;
class Map;
class PrimOp;
class Slot;
class Store;
class Unmap;

/**
 * The World represents the whole program and manages creation and destruction of AIR nodes.
 * In particular, the following things are done by this class:
 *
 *  - \p Type unification: \n
 *      There exists only one unique \p Type.
 *      These \p Type%s are hashed into an internal map for fast access.
 *      The getters just calculate a hash and lookup the \p Type, if it is already present, or create a new one otherwise.
 *  - Value unification: \n
 *      This is a built-in mechanism for the following things:
 *      - constant pooling
 *      - constant folding
 *      - common subexpression elimination
 *      - canonicalization of expressions
 *      - several local optimizations
 *
 *  \p PrimOp%s do not explicitly belong to a Lambda.
 *  Instead they either implicitly belong to a Lambda--when
 *  they (possibly via multiple levels of indirection) depend on a Lambda's Param--or they are dead.
 *  Use \p cleanup to remove dead code and unreachable code.
 *
 *  You can create several worlds.
 *  All worlds are completely independent from each other.
 *  This is particular useful for multi-threading.
 */
class World {
private:
    World& operator = (const World&); ///< Do not copy-assign a \p World instance.
    World(const World&);              ///< Do not copy-construct a \p World.

    struct TypeHash { size_t operator () (const TypeNode* t) const { return t->hash(); } };
    struct TypeEqual { bool operator () (const TypeNode* t1, const TypeNode* t2) const { return t1->equal(t2); } };

public:
    typedef HashSet<const PrimOp*, PrimOpHash, PrimOpEqual> PrimOps;
    typedef HashSet<const TypeNode*, TypeHash, TypeEqual> Types;

    World(std::string name = "");
    ~World();

    // types

#define THORIN_ALL_TYPE(T, M) PrimType type_##T(size_t length = 1) { \
    return length == 1 ? PrimType(T##_) : join(new PrimTypeNode(*this, PrimType_##T, length)); \
}
#include "thorin/tables/primtypetable.h"

    /// Get PrimType.
    PrimType    type(PrimTypeKind kind, size_t length = 1) {
        size_t i = kind - Begin_PrimType;
        assert(0 <= i && i < (size_t) Num_PrimTypes);
        return length == 1 ? PrimType(primtypes_[i]) : join(new PrimTypeNode(*this, kind, length));
    }
    MemType     mem_type() const { return MemType(mem_); }
    FrameType   frame_type() const { return FrameType(frame_); }
    PtrType     ptr_type(Type referenced_type, size_t length = 1, int32_t device = -1, AddressSpace adr_space = AddressSpace::Generic) {
        return join(new PtrTypeNode(*this, referenced_type, length, device, adr_space));
    }
    TupleType           tuple_type() { return tuple0_; } ///< Returns unit, i.e., an empty \p TupleType.
    TupleType           tuple_type(ArrayRef<Type> args) { return join(new TupleTypeNode(*this, args)); }
    StructAbsType       struct_abs_type(size_t size, const std::string& name = "") {
        return join(new StructAbsTypeNode(*this, size, name));
    }
    StructAppType       struct_app_type(StructAbsType struct_abs_type, ArrayRef<Type> args) {
        return join(new StructAppTypeNode(struct_abs_type, args));
    }
    FnType              fn_type() { return fn0_; }       ///< Returns an empty \p FnType.
    FnType              fn_type(ArrayRef<Type> args) { return join(new FnTypeNode(*this, args)); }
    TypeVar             type_var() { return join(new TypeVarNode(*this)); }
    DefiniteArrayType   definite_array_type(Type elem, u64 dim) { return join(new DefiniteArrayTypeNode(*this, elem, dim)); }
    IndefiniteArrayType indefinite_array_type(Type elem) { return join(new IndefiniteArrayTypeNode(*this, elem)); }

    // literals

#define THORIN_ALL_TYPE(T, M) \
    Def literal_##T(T val, size_t length = 1) { return literal(PrimType_##T, Box(val), length); }
#include "thorin/tables/primtypetable.h"
    Def literal(PrimTypeKind kind, Box value, size_t length = 1);
    Def literal(PrimTypeKind kind, int64_t value, size_t length = 1);
    template<class T>
    Def literal(T value, size_t length = 1) { return literal(type2kind<T>::kind, Box(value), length); }
    Def zero(PrimTypeKind kind, size_t length = 1) { return literal(kind, 0, length); }
    Def zero(Type, size_t length = 1);
    Def one(PrimTypeKind kind, size_t length = 1) { return literal(kind, 1, length); }
    Def one(Type, size_t length = 1);
    Def allset(PrimTypeKind kind, size_t length = 1) { return literal(kind, -1, length); }
    Def allset(Type, size_t length = 1);
    Def any(Type type, size_t length = 1);
    Def any(PrimTypeKind kind, size_t length = 1) { return any(type(kind), length); }
    Def bottom(Type type, size_t length = 1);
    Def bottom(PrimTypeKind kind, size_t length = 1) { return bottom(type(kind), length); }
    /// Creates a vector of all true while the length is derived from @p def.
    Def true_mask(Def def) { return literal(true, def->length()); }
    Def true_mask(size_t length) { return literal(true, length); }
    Def false_mask(Def def) { return literal(false, def->length()); }
    Def false_mask(size_t length) { return literal(false, length); }

    // arithops

    /// Creates an \p ArithOp or a \p Cmp.
    Def binop(int kind, Def cond, Def lhs, Def rhs, const std::string& name = "");
    Def binop(int kind, Def lhs, Def rhs, const std::string& name = "") {
        return binop(kind, true_mask(lhs), lhs, rhs, name);
    }

    Def arithop(ArithOpKind kind, Def cond, Def lhs, Def rhs, const std::string& name = "");
    Def arithop(ArithOpKind kind, Def lhs, Def rhs, const std::string& name = "") {
        return arithop(kind, true_mask(lhs), lhs, rhs, name);
    }
#define THORIN_ARITHOP(OP) \
    Def arithop_##OP(Def cond, Def lhs, Def rhs, const std::string& name = "") { \
        return arithop(ArithOp_##OP, cond, lhs, rhs, name); \
    } \
    Def arithop_##OP(Def lhs, Def rhs, const std::string& name = "") { \
        return arithop(ArithOp_##OP, true_mask(lhs), lhs, rhs, name); \
    }
#include "thorin/tables/arithoptable.h"

    Def arithop_not(Def cond, Def def);
    Def arithop_not(Def def) { return arithop_not(true_mask(def), def); }
    Def arithop_minus(Def cond, Def def);
    Def arithop_minus(Def def) { return arithop_minus(true_mask(def), def); }

    // compares

    Def cmp(CmpKind kind, Def cond, Def lhs, Def rhs, const std::string& name = "");
    Def cmp(CmpKind kind, Def lhs, Def rhs, const std::string& name = "") {
        return cmp(kind, true_mask(lhs), lhs, rhs, name);
    }
#define THORIN_CMP(OP) \
    Def cmp_##OP(Def cond, Def lhs, Def rhs, const std::string& name = "") { \
        return cmp(Cmp_##OP, cond, lhs, rhs, name);  \
    } \
    Def cmp_##OP(Def lhs, Def rhs, const std::string& name = "") { \
        return cmp(Cmp_##OP, true_mask(lhs), lhs, rhs, name);  \
    }
#include "thorin/tables/cmptable.h"

    // casts

    Def convert(Def cond, Def from, Type to, const std::string& name = "");
    Def convert(Def from, Type to, const std::string& name = "") { return convert(true_mask(from), from, to, name); }
    Def cast(Def cond, Def from, Type to, const std::string& name = "");
    Def cast(Def from, Type to, const std::string& name = "") { return cast(true_mask(from), from, to, name); }
    Def bitcast(Def cond, Def from, Type to, const std::string& name = "");
    Def bitcast(Def from, Type to, const std::string& name = "") { return bitcast(true_mask(from), from, to, name); }

    // aggregate operations

    Def definite_array(Type elem, ArrayRef<Def> args, const std::string& name = "") {
        return cse(new DefiniteArray(*this, elem, args, name));
    }
    /// Create definite_array with at least one element. The type of that element is the element type of the definite array.
    Def definite_array(ArrayRef<Def> args, const std::string& name = "") {
        assert(!args.empty());
        return definite_array(args.front()->type(), args, name);
    }
    Def indefinite_array(Type elem, Def dim, const std::string& name = "") {
        return cse(new IndefiniteArray(*this, elem, dim, name));
    }
    Def struct_agg(StructAppType struct_app_type, ArrayRef<Def> args, const std::string& name = "") {
        return cse(new StructAgg(*this, struct_app_type, args, name));
    }
    Def tuple(ArrayRef<Def> args, const std::string& name = "") { return cse(new Tuple(*this, args, name)); }
    Def vector(ArrayRef<Def> args, const std::string& name = "") {
        if (args.size() == 1) return args[0];
        return cse(new Vector(*this, args, name));
    }
    /// Splats \p arg to create a \p Vector with \p length.
    Def vector(Def arg, size_t length = 1, const std::string& name = "");
    Def extract(Def tuple, Def index, const std::string& name = "");
    Def extract(Def tuple, u32 index, const std::string& name = "");
    Def insert(Def tuple, Def index, Def value, const std::string& name = "");
    Def insert(Def tuple, u32 index, Def value, const std::string& name = "");

    // memory stuff

    const Enter* enter(Def mem, const std::string& name = "");
    Def leave(Def mem, Def frame, const std::string& name = "");
    const Slot* slot(Type type, Def frame, size_t index, const std::string& name = "");
    Def alloc(Def mem, Type type, Def extra, const std::string& name = "");
    Def alloc(Def mem, Type type, const std::string& name = "") { return alloc(mem, type, literal_qu64(0), name); }
    const Global* global(Def init, bool is_mutable = true, const std::string& name = "");
    const Global* global_immutable_string(const std::string& str, const std::string& name = "");
    Def load(Def mem, Def ptr, const std::string& name = "");
    const Store* store(Def mem, Def ptr, Def val, const std::string& name = "");
    const LEA* lea(Def ptr, Def index, const std::string& name = "");
    const Map* map(Def mem, Def ptr, Def device, Def addr_space, Def mem_offset, Def mem_size, const std::string& name = "");
    const Map* map(Def mem, Def ptr, uint32_t device, AddressSpace addr_space, Def mem_offset, Def mem_size, const std::string& name = "");
    const Unmap* unmap(Def mem, Def ptr, Def device, Def addr_space, const std::string& name = "");
    const Unmap* unmap(Def mem, Def ptr, uint32_t device, AddressSpace addr_space, const std::string& name = "");

    // guided partial evaluation

    Def run(Def def, const std::string& name = "");
    Def hlt(Def def, const std::string& name = "");
    Def end_run(Def def, Def run, const std::string& name = "");
    Def end_hlt(Def def, Def hlt, const std::string& name = "");

    /// Select is higher-order. You can build branches with a \p Select primop.
    Def select(Def cond, Def t, Def f, const std::string& name = "");

    // lambdas

    Lambda* lambda(FnType fn, Lambda::Attribute attribute = Lambda::Attribute(0), Lambda::Attribute intrinsic = Lambda::Attribute(0), const std::string& name = "");
    Lambda* lambda(FnType fn, const std::string& name) { return lambda(fn, Lambda::Attribute(0), Lambda::Attribute(0), name); }
    Lambda* lambda(const std::string& name) { return lambda(fn_type(), Lambda::Attribute(0), Lambda::Attribute(0), name); }
    Lambda* basicblock(const std::string& name = "");
    Lambda* meta_lambda();

    /// Generic \p PrimOp constructor; inherits name from \p in.
    static Def rebuild(World& to, const PrimOp* in, ArrayRef<Def> ops, Type type);
    /// Generic \p PrimOp constructor; inherits type and name name from \p in.
    static Def rebuild(World& to, const PrimOp* in, ArrayRef<Def> ops) { return rebuild(to, in, ops, in->type()); }
    /// Generic \p Type constructor.
    static Type rebuild(World& to, Type in, ArrayRef<Type> args);

    Def rebuild(const PrimOp* in, ArrayRef<Def> ops, Type type) { return rebuild(*this, in, ops, type); }
    Def rebuild(const PrimOp* in, ArrayRef<Def> ops) { return rebuild(in, ops, in->type()); }
    Type rebuild(Type in, ArrayRef<Type> args) { return rebuild(*this, in, args); }

    /// Performs dead code, unreachable code and unused type elimination.
    void cleanup();
    void opt();

    // getters

    const std::string& name() const { return name_; }
    const PrimOps& primops() const { return primops_; }
    const LambdaSet& lambdas() const { return lambdas_; }
    Array<Lambda*> copy_lambdas() const;
    std::vector<Lambda*> externals() const;
    const Types& types() const { return types_; }
    size_t gid() const { return gid_; }

    // other stuff

    void destroy(Lambda* lambda);
#ifndef NDEBUG
    void breakpoint(size_t number) { breakpoints_.insert(number); }
#endif
    const TypeNode* unify_base(const TypeNode*);
    template<class T> Proxy<T> unify(const T* type) { return Proxy<T>(unify_base(type)->template as<T>()); }

private:
    const TypeNode* register_base(const TypeNode* type) {
        assert(type->gid_ == size_t(-1));
        type->gid_ = gid_++;
        garbage_.push_back(type);
        return type;
    }
    template<class T> Proxy<T> join(const T* t) { return Proxy<T>(register_base(t)->template as<T>()); }
    const DefNode* cse_base(const PrimOp*);
    template<class T> const T* cse(const T* primop) { return cse_base(primop)->template as<T>(); }

    const Param* param(Type type, Lambda* lambda, size_t index, const std::string& name = "");

    std::string name_;
    LambdaSet lambdas_;
    PrimOps primops_;
    Types types_;
    std::vector<const TypeNode*> garbage_;
#ifndef NDEBUG
    HashSet<size_t> breakpoints_;
#endif

    size_t gid_;

    union {
        struct {
            const TupleTypeNode* tuple0_;///< tuple().
            const FnTypeNode*    fn0_;   ///< fn().
            const MemTypeNode*   mem_;
            const FrameTypeNode* frame_;

            union {
                struct {
#define THORIN_ALL_TYPE(T, M) const PrimTypeNode* T##_;
#include "thorin/tables/primtypetable.h"
                };

                const PrimTypeNode* primtypes_[Num_PrimTypes];
            };
        };

        const TypeNode* keep_[Num_PrimTypes + 4];
    };

    friend class Cleaner;
    friend class Lambda;
};

}

#endif
