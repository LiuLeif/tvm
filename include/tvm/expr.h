/*!
 *  Copyright (c) 2016 by Contributors
 * \file expr.h
 * \brief The Expr and related elements in DataFlow construction.
 */
#ifndef TVM_EXPR_H_
#define TVM_EXPR_H_

#include <ir/Expr.h>
#include <ir/IRPrinter.h>
#include <ir/IROperator.h>
#include <string>
#include <algorithm>
#include "./base.h"
#include "./runtime/c_runtime_api.h"

namespace tvm {

using Halide::Type;
using Halide::Float;
using Halide::Bool;
using Halide::Int;
using Halide::UInt;
using Halide::Handle;
using Halide::ExprHash;
using Halide::ExprEqual;

using Halide::Expr;
using Halide::VarExpr;
using Halide::IR::FunctionRef;
using Halide::IR::FunctionBaseNode;
using Halide::Internal::Stmt;
using Halide::Internal::IRPrinter;
using Halide::Internal::Variable;

using Halide::Internal::make_const;
using Halide::Internal::make_zero;
using Halide::Internal::as_const_int;
using Halide::Internal::as_const_uint;
using Halide::Internal::const_true;
using Halide::Internal::const_false;

inline Type TVMType2Type(TVMType t) {
  return Type(static_cast<halide_type_code_t>(t.code), t.bits, t.lanes);
}

inline TVMType Type2TVMType(Type t) {
  TVMType ret;
  ret.code = static_cast<uint8_t>(t.code());
  ret.bits = static_cast<uint8_t>(t.bits());
  ret.lanes = static_cast<uint16_t>(t.lanes());
  return ret;
}

/*! \brief a named variable in TVM */
class Var : public Halide::VarExpr {
 public:
  explicit Var(const std::string& name_hint = "v",
               Type t = Int(32)) : VarExpr(name_hint, t) {}
  explicit Var(std::shared_ptr<Node> n) : VarExpr(n) {}
  explicit Var(VarExpr v) : VarExpr(v) {}
  /*!
   * \brief Make a new copy of var with same type, append suffix
   * \param suffix The suffix to be appended.
   * \return the new Var copy
   */
  Var copy_with_suffix(const std::string& suffix) const {
    return Var((*this)->name_hint + suffix, (*this)->type);
  }
  /*! \brief type indicate the container type */
  using ContainerType = Variable;
};


/*! \brief container class of iteration variable. */
class IterVarNode;

/*!
 * \brief same as Halide::IR::Range
 *  except it provide an constructor with (begin, end)
 *
 *  \note Traditional Halide's Range have a constructor with
 *   (begin, extent), which does not match the convention in e.g. python.
 *   We decided to correct it by removing the constructor in HalideIR,
 *   and add it back in TVM's range.
 */
class Range : public Halide::IR::Range {
 public:
  /*! \brief constructor */
  Range() {}
  explicit Range(std::shared_ptr<Node> n) : Halide::IR::Range(n) {}
  /*!
   * \brief constructor by begin and end
   * \param begin The begin of the range.
   * \param end The end of the range.
   */
  Range(Expr begin, Expr end);

  static Range make_with_min_extent(Expr min, Expr extent);
};

/*!
 * \brief Type of iteration variable.
 *  Each IterVar have a specific type.
 *
 *  The type of iter var can be overriden via
 *  stage.iter_var_attrs given they are compatible.
 */
enum IterVarType : int {
  /*!
   * \brief Data parallel iteration.
   *  This normally corresponds to axis of Tensor.
   *  Allow all IterVar manipulations.
   *
   * \note This does not mean the loop
   *  have to be executed in parallel fashion.
   */
  kDataPar = 0,
  /*!
   * \brief The IterVar itself is a thread-index
   *  of a fixed thread launching group.
   *  Note that this is already assumed to be paralellized.
   *
   *  Disallow: split/fuse/vectorize/parallel
   */
  kThreadIndex = 1,
  /*!
   * \brief Communicative reduction.
   *  Cannot be directly parallelized.
   *
   *  Disallow: parallel/vectorize
   */
  kCommReduce = 2,
  /*!
   * \brief Serial loops with loop carry dependency,
   *  the iteration must execute in order.
   *  Cannot be re-ordered.
   *
   *  Disallow: reorder/parallel/vectorize
   */
  kOrdered = 3,
  /*!
   * \brief IterVar is opaque,
   *
   *  May not corresponds to any generated loop
   *  Disallow all IterVar manipulations and compute_at
   *
   * \note This is usually used to implement composite op
   *  or external op, where the
   */
  kOpaque = 4,
  // The following are possible additional
  // types that are provided during schedule
  /*!
   * \brief The execution is unrolled.
   */
  kUnrolled = 5,
  /*!
   * \brief The loop is vectorized.
   */
  kVectorized = 6,
  /*!
   * \brief The loop is parallelized.
   */
  kParallelized = 7
};

/*!
 * \brief Iteration Variable,
 *  represents an iteration over an integer interval.
 */
class IterVar : public NodeRef {
 public:
  // construct a new iter var without a domain
  IterVar() {}
  // construct from shared ptr.
  explicit IterVar(std::shared_ptr<Node> n) : NodeRef(n) {}
  /*!
   * \brief access the internal node container
   * \return the pointer to the internal node container
   */
  inline const IterVarNode* operator->() const;
  /*!
   * \return the corresponding var in the IterVar.
   */
  inline operator Expr() const;
  /*! \brief specify container node */
  using ContainerType = IterVarNode;
};

/*!
 * \brief Create a new IterVar that represents an axis in thread.
 *
 * \param dom Optional, domain of the thread axis.
 * \param tag The thread tag of the axis.
 */
IterVar thread_axis(Range dom, std::string tag);

/*!
 * \brief Create a new IterVar for reduction operations.
 *
 * \param dom The domain of the reduction axis.
 * \param name The name of the reduction axis.
 */
IterVar reduce_axis(Range dom, std::string name = "rv");

using Domain = Array<Range>;

// functions
using Halide::cast;
using Halide::min;
using Halide::max;
using Halide::abs;
using Halide::select;

/*!
 * \brief sum of of source expression over axis
 * \param source The source expression.
 * \param axis List of iteration variables that will be used for reduction.
 */
Expr sum(Expr source, Array<IterVar> axis);

/*!
 * \brief max of of source expression over axis
 * \param source The source expression.
 * \param axis List of iteration variables that will be used for reduction.
 */
Expr max(Expr source, Array<IterVar> axis);

/*!
 * \brief max of of source expression over axis
 * \param source The source expression.
 * \param axis List of iteration variables that will be used for reduction.
 */
Expr min(Expr source, Array<IterVar> axis);


// print functions for expr
std::ostream& operator<<(std::ostream& os, const NodeRef& n);  // NOLINT(*)

// definition of Node.
/*!
 * \brief An iteration variable representing an iteration
 *  over a one dimensional interval.
 */
class IterVarNode : public Node {
 public:
  /*!
   * \brief the domain of iteration, if known, can be None
   *  For the intermediate schedule node, before schedule.
   */
  Range dom;
  /*! \brief The looping variable */
  Var var;
  /*! \brief The type of the IterVar */
  IterVarType iter_type;
  /*!
   * \brief additional tag on the iteration variable,
   *  set this if this is binded already to a known thread tag.
   */
  std::string thread_tag;

  void VisitAttrs(AttrVisitor* v) final {
    v->Visit("dom", &dom);
    v->Visit("var", &var);
    v->Visit("iter_type", &iter_type);
    v->Visit("thread_tag", &thread_tag);
  }

  static IterVar make(Range dom, Var var,
                      IterVarType iter_type,
                      std::string thread_tag = "");

  static constexpr const char* _type_key = "IterVar";
  TVM_DECLARE_NODE_TYPE_INFO(IterVarNode, Node);
};

// inline implementations
inline const IterVarNode* IterVar::operator->() const {
  return static_cast<const IterVarNode*>(node_.get());
}

inline IterVar::operator Expr() const {
  return (*this)->var;
}

inline const char* IterVarType2String(IterVarType t) {
  switch (t) {
    case kDataPar: return "DataPar";
    case kThreadIndex: return "ThreadIndex";
    case kCommReduce: return "CommRedude";
    case kOrdered: return "Ordered";
    case kOpaque: return "Opaque";
    case kUnrolled: return "Unrolled";
    case kVectorized: return "Vectorized";
    case kParallelized: return "Parallelized";
  }
  return "Unknown";
}

}  // namespace tvm

namespace std {
template <>
struct hash<::tvm::IterVar> {
  std::size_t operator()(const ::tvm::IterVar& k) const {
    return k.hash();
  }
};
}
#endif  // TVM_EXPR_H_
