#include <cassert>
#include <memory>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <libxml/tree.h>
#include "SymbolAnalyzer.h"
#include "XcodeMlType.h"
#include "XcodeMlEnvironment.h"
#include "TypeAnalyzer.h"

#include <iostream>

namespace XcodeMl {

Type::Type(std::string id):
  ident(id)
{}

Type::~Type() {}

DataTypeIdent Type::dataTypeIdent() {
  return ident;
}

Reserved::Reserved(DataTypeIdent ident, std::string dataType):
  Type(ident),
  name(dataType)
{}

std::string Reserved::makeDeclaration(std::string var, const Environment&) {
  return name + " " + var;
}

TypeKind Reserved::getKind() {
  return TypeKind::Reserved;
}

Reserved::~Reserved() = default;

Pointer::Pointer(DataTypeIdent ident, TypeRef signified):
  Type(ident),
  ref(signified->dataTypeIdent())
{}

Pointer::Pointer(DataTypeIdent ident, DataTypeIdent signified):
  Type(ident),
  ref(signified)
{}

std::string Pointer::makeDeclaration(std::string var, const Environment& env) {
  auto refType = env[ref];
  if (!refType) {
    return "INCOMPLETE_TYPE *" + var;
  }
  switch (typeKind(refType)) {
    case TypeKind::Function:
      return makeDecl(refType, "(*" + var + ")", env);
    default:
      return makeDecl(refType, "*" + var, env);
  }
}

TypeKind Pointer::getKind() {
  return TypeKind::Pointer;
}

Pointer::~Pointer() = default;

Function::Function(DataTypeIdent ident, TypeRef r, const std::vector<DataTypeIdent>& p):
  Type(ident),
  returnValue(r->dataTypeIdent()),
  params(p.size())
{
  // FIXME: initialization cost
  for (size_t i = 0; i < p.size(); ++i) {
    params[i] = std::make_tuple(p[i], "");
  }
}

Function::Function(DataTypeIdent ident, TypeRef r, const std::vector<std::tuple<DataTypeIdent, std::string>>& p):
  Type(ident),
  returnValue(r->dataTypeIdent()),
  params(p)
{}

std::string Function::makeDeclaration(std::string var, const Environment& env) {
  std::stringstream ss;
  auto returnType(env[returnValue]);
  if (!returnType) {
    return "INCOMPLETE_TYPE *" + var;
  }
  ss << makeDecl(returnType, "", env)
    << " "
    << var
    << "(";
  bool alreadyPrinted = false;
  for (auto param : params) {
    auto paramDTI(std::get<0>(param));
    auto paramType(env[paramDTI]);
    auto paramName(std::get<1>(param));
    if (!paramType) {
      return "INCOMPLETE_TYPE *" + var;
    }
    ss << (alreadyPrinted ? ", " : "")
      << makeDecl(paramType, paramName, env);
    alreadyPrinted = true;
  }
  ss <<  ")";
  return ss.str();
}

TypeKind Function::getKind() {
  return TypeKind::Function;
}

Function::~Function() = default;

Array::Array(DataTypeIdent ident, TypeRef elem, size_t s):
  Type(ident),
  element(elem->dataTypeIdent()),
  size(std::make_shared<size_t>(s))
{}

std::string Array::makeDeclaration(std::string var, const Environment& env) {
  auto elementType(env[element]);
  if (!elementType) {
    return "INCOMPLETE_TYPE *" + var;
  }
  return makeDecl(elementType, var + "[]", env);
}

TypeKind Array::getKind() {
  return TypeKind::Pointer;
}

Array::~Array() = default;

Struct::Struct(DataTypeIdent ident, std::string n, std::string t, SymbolMap &&f)
  : Type(ident), name(n), tag(t), fields(f) {
  std::cerr << "Struct::Struct(" << n << ")" << std::endl;
}

std::string Struct::makeDeclaration(std::string var, const Environment&)
{
  std::stringstream ss;
  ss << "struct " << name << " " << var;
  return ss.str();
}

Struct::~Struct() = default;

TypeKind Struct::getKind() {
  return TypeKind::Struct;
}

/*!
 * \brief Return the kind of \c type.
 */
TypeKind typeKind(TypeRef type) {
  return type->getKind();
}

std::string makeDecl(TypeRef type, std::string var, const Environment& env) {
  if (type) {
    return type->makeDeclaration(var, env);
  } else {
    return "UNKNOWN_TYPE";
  }
}

TypeRef makeReservedType(DataTypeIdent ident, std::string name) {
  return std::make_shared<Reserved>(
      ident,
      name
  );
}

TypeRef makePointerType(DataTypeIdent ident, TypeRef ref) {
  return std::make_shared<Pointer>(
      ident,
      ref
  );
}

TypeRef makePointerType(DataTypeIdent ident, DataTypeIdent ref) {
  return std::make_shared<Pointer>(ident, ref);
}

TypeRef makeFunctionType(
    DataTypeIdent ident,
    TypeRef returnType,
    const Function::Params& params
) {
  return std::make_shared<Function>(
      ident,
      returnType,
      params
  );
}

TypeRef makeArrayType(
    DataTypeIdent ident,
    TypeRef elemType,
    size_t size
) {
  return std::make_shared<Array>(
      ident,
      elemType,
      size
  );
}

TypeRef makeStructType(
    DataTypeIdent ident,
    std::string name,
    std::string tag,
    SymbolMap&& fields
) {
  return std::make_shared<Struct>(ident, name, tag, std::move(fields));
}

std::string TypeRefToString(TypeRef type, const Environment& env) {
  return makeDecl(type, "", env);
}

}
