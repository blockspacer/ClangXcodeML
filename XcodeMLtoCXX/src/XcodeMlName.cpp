#include <map>
#include <memory>
#include <string>
#include <vector>
#include <libxml/tree.h>
#include "llvm/ADT/Optional.h"
#include "llvm/Support/Casting.h"
#include "StringTree.h"
#include "XcodeMlNns.h"
#include "XcodeMlType.h"
#include "XcodeMlTypeTable.h"
#include "XcodeMlOperator.h"

#include "XcodeMlName.h"

using XcodeMl::CodeFragment;
using CXXCodeGen::makeTokenNode;
using CXXCodeGen::makeVoidNode;

namespace XcodeMl {

Name::Name(const CodeFragment &spec, const std::shared_ptr<UnqualId> &id_)
    : nestedNameSpec(spec), id(id_) {
}

Name::Name(const std::shared_ptr<UnqualId> &id_) : nestedNameSpec(), id(id_) {
}

CodeFragment
Name::toString(const TypeTable &typeTable, const NnsTable &) const {
  assert(id);
  if (nestedNameSpec) {
    return nestedNameSpec + id->toString(typeTable);
  }
  return id->toString(typeTable);
}

std::shared_ptr<UnqualId>
Name::getUnqualId() const {
  assert(id);
  auto pId = id->clone();
  return std::shared_ptr<UnqualId>(pId);
}

UnqualId::UnqualId(UnqualIdKind k) : kind(k) {
}

UnqualId::~UnqualId() {
}

UnqualIdKind
UnqualId::getKind() const {
  return kind;
}

UIDIdent::UIDIdent(const std::string &id)
    : UnqualId(UnqualIdKind::Ident), ident(id) {
}

UnqualId *
UIDIdent::clone() const {
  auto copy = new UIDIdent(*this);
  return copy;
}

CodeFragment
UIDIdent::toString(const TypeTable &, const NnsTable &) const {
  return makeTokenNode(ident);
}

bool
UIDIdent::classof(const UnqualId *id) {
  return id->getKind() == UnqualIdKind::Ident;
}

OpFuncId::OpFuncId(const std::string &op)
    : UnqualId(UnqualIdKind::OpFuncId), opSpelling(op) {
}

UnqualId *
OpFuncId::clone() const {
  auto copy = new OpFuncId(*this);
  return copy;
}

CodeFragment
OpFuncId::toString(const TypeTable &) const {
  return makeTokenNode("operator") + makeTokenNode(opSpelling);
}

bool
OpFuncId::classof(const UnqualId *id) {
  return id->getKind() == UnqualIdKind::OpFuncId;
}

ConvFuncId::ConvFuncId(const DataTypeIdent &type)
    : UnqualId(UnqualIdKind::ConvFuncId), dtident(type) {
}

UnqualId *
ConvFuncId::clone() const {
  auto copy = new ConvFuncId(*this);
  return copy;
}

CodeFragment
ConvFuncId::toString(const TypeTable &env) const {
  const auto T = env.at(dtident);
  return makeTokenNode("operator") + T->makeDeclaration(makeVoidNode(), env);
}

bool
ConvFuncId::classof(const UnqualId *id) {
  return id->getKind() == UnqualIdKind::ConvFuncId;
}

CtorName::CtorName(const DataTypeIdent &d)
    : UnqualId(UnqualIdKind::Ctor), dtident(d) {
}

UnqualId *
CtorName::clone() const {
  auto copy = new CtorName(*this);
  return copy;
}

CodeFragment
CtorName::toString(const TypeTable &env) const {
  const auto T = env.at(dtident);
  const auto ClassT = llvm::cast<XcodeMl::ClassType>(T.get());
  const auto name = ClassT->name();
  return name;
}

bool
CtorName::classof(const UnqualId *id) {
  return id->getKind() == UnqualIdKind::Ctor;
}

DtorName::DtorName(const DataTypeIdent &d)
    : UnqualId(UnqualIdKind::Dtor), dtident(d) {
}

UnqualId *
DtorName::clone() const {
  auto copy = new DtorName(*this);
  return copy;
}

CodeFragment
DtorName::toString(const TypeTable &env) const {
  const auto T = env.at(dtident);
  const auto ClassT = llvm::cast<XcodeMl::ClassType>(T.get());
  const auto name = ClassT->name();
  return makeTokenNode("~") + name;
}

bool
DtorName::classof(const UnqualId *id) {
  return id->getKind() == UnqualIdKind::Dtor;
}

} // namespace XcodeMl
