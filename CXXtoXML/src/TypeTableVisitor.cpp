#include "XMLVisitorBase.h"
#include "DeclarationsVisitor.h"
#include "TypeTableVisitor.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <cctype>

using namespace clang;
using namespace llvm;

static cl::opt<bool>
OptTraceTypeTable("trace-typeTable",
                  cl::desc("emit traces on <typeTable>"),
                  cl::cat(CXX2XMLCategory));
static cl::opt<bool>
OptFullTraceTypeTable("fulltrace-typeTable",
                      cl::desc("emit full-traces on <typeTable>"),
                      cl::cat(CXX2XMLCategory));
static cl::opt<bool>
OptDisableTypeTable("disable-typeTable",
                    cl::desc("disable <typeTable>"),
                    cl::cat(CXX2XMLCategory));

static cl::opt<bool>
OptIgnoreUnknownType("ignore-unknown-type",
                      cl::desc("ignore unknown type"),
                      cl::cat(CXX2XMLCategory));

static cl::opt<std::string>
OptTypeNameMap("typenamemap",
               cl::desc("a map file of typename substitution"),
               cl::cat(CXX2XMLCategory));

static std::ifstream mapfile;
static bool map_is_already_set = false;
static std::map<std::string, std::string> typenamemap;

static std::string
make_comment(Decl *decl, std::string message) {
  std::string comment("PreVisitDecl::");
  comment += decl->getDeclKindName();
  comment += ": ";
  comment += message;
  return comment;
}

TypeTableInfo::TypeTableInfo(MangleContext *MC, InheritanceInfo *II) :
  mangleContext(MC),
  inheritanceinfo(II)
{
  mapFromNameToQualType.clear();
  mapFromQualTypeToName.clear();
  mapFromQualTypeToXmlNodePtr.clear();
  seqForBasicType = 0;
  seqForPointerType = 0;
  seqForFunctionType = 0;
  seqForArrayType = 0;
  seqForStructType = 0;
  seqForUnionType = 0;
  seqForEnumType = 0;
  seqForClassType = 0;
  seqForOtherType = 0;

  basicTypeNodes.clear();
  pointerTypeNodes.clear();
  functionTypeNodes.clear();
  arrayTypeNodes.clear();
  structTypeNodes.clear();
  unionTypeNodes.clear();
  enumTypeNodes.clear();
  classTypeNodes.clear();
  otherTypeNodes.clear();

  useLabelType = false;
}

std::string TypeTableInfo::registerBasicType(QualType T){
  std::string name = mapFromQualTypeToName[T];
  assert(name.empty());

  raw_string_ostream OS(name);
  OS << "Basic" << seqForBasicType++;
  return mapFromQualTypeToName[T] = OS.str();
}

std::string TypeTableInfo::registerPointerType(QualType T){
  std::string name = mapFromQualTypeToName[T];
  assert(name.empty());

  raw_string_ostream OS(name);
  switch (T->getTypeClass()) {
  case Type::Pointer:
  case Type::BlockPointer:
  case Type::LValueReference:
  case Type::RValueReference:
  case Type::MemberPointer:
    OS << "Pointer" << seqForPointerType++;
    break;
  default:
    abort();
  }
  return mapFromQualTypeToName[T] = OS.str();
}

std::string TypeTableInfo::registerFunctionType(QualType T){
  assert(T->getTypeClass() == Type::FunctionProto
         || T->getTypeClass() == Type::FunctionNoProto);
  std::string name = mapFromQualTypeToName[T];
  assert(name.empty());

  raw_string_ostream OS(name);
  OS << "Function" << seqForFunctionType++;
  return mapFromQualTypeToName[T] = OS.str();
}

std::string TypeTableInfo::registerArrayType(QualType T){
  std::string name = mapFromQualTypeToName[T];
  assert(name.empty());

  raw_string_ostream OS(name);
  switch (T->getTypeClass()) {
  case Type::ConstantArray:
    OS << "ConstantArray" << seqForArrayType++;
    break;
  case Type::IncompleteArray:
    OS << "ImcompleteArray" << seqForArrayType++;
    break;
  case Type::VariableArray:
    OS << "VariableArray" << seqForArrayType++;
    break;
  case Type::DependentSizedArray:
    OS << "DependentSizeArray" << seqForArrayType++;
    break;
  default:
    abort();
  }
  return mapFromQualTypeToName[T] = OS.str();
}

std::string TypeTableInfo::registerRecordType(QualType T){
  assert(T->getTypeClass() == Type::Record);
  std::string name = mapFromQualTypeToName[T];
  assert(name.empty());

  raw_string_ostream OS(name);
  if (T->isStructureType()) {
    OS << "Struct" << seqForStructType++;
  } else if (T->isUnionType()) {
    OS << "Union" << seqForUnionType++;
  } else if (T->isClassType()) {
    OS << "Class" << seqForClassType++;
  } else {
    abort();
  }
  return mapFromQualTypeToName[T] = OS.str();
}

std::string TypeTableInfo::registerEnumType(QualType T){
  assert(T->getTypeClass() == Type::Enum);
  std::string name = mapFromQualTypeToName[T];
  assert(name.empty());

  raw_string_ostream OS(name);
  OS << "Enum" << seqForEnumType++;
  return mapFromQualTypeToName[T] = OS.str();
}

std::string TypeTableInfo::registerOtherType(QualType T){
  std::string name = mapFromQualTypeToName[T];
  assert(name.empty());

  raw_string_ostream OS(name);
  OS << "Other" << seqForOtherType++;
  return mapFromQualTypeToName[T] = OS.str();
}

xmlNodePtr TypeTableInfo::createNode(QualType T, const char *fieldname, xmlNodePtr) {
  xmlNodePtr N = xmlNewNode(nullptr, BAD_CAST fieldname);
  bool isQualified = false;
  xmlNewProp(N, BAD_CAST "type", BAD_CAST getTypeName(T).c_str());

  if (T.isConstQualified()) {
    xmlNewProp(N, BAD_CAST "is_const", BAD_CAST "1");
    isQualified = true;
  }
  if (T.isVolatileQualified()) {
    xmlNewProp(N, BAD_CAST "is_volatile", BAD_CAST "1");
    isQualified = true;
  }
  if (T.isRestrictQualified()) {
    xmlNewProp(N, BAD_CAST "is_restrict", BAD_CAST "1");
    isQualified = true;
  }
  if (isQualified) {
    registerType(T.getUnqualifiedType(), nullptr, nullptr);
    xmlNewProp(N, BAD_CAST "name",
               BAD_CAST getTypeName(T.getUnqualifiedType()).c_str());
  }

  return N;
}

void TypeTableInfo::registerType(QualType T, xmlNodePtr *retNode, xmlNodePtr) {
  bool isQualified = false;
  xmlNodePtr Node = nullptr;
  std::string rawname;

  if (T.isNull()) {
    return;
  };

  if (!T.isCanonical()) {
    registerType(T.getCanonicalType(), retNode, nullptr);
    mapFromQualTypeToName[T] = mapFromQualTypeToName[T.getCanonicalType()];
    return;
  }

  if (!mapFromQualTypeToName[T].empty()) {
    if (retNode != nullptr) {
      *retNode = mapFromQualTypeToXmlNodePtr[T];
    }
    return;
  }

  if (T.isConstQualified()) {
    isQualified = true;
  }
  if (T.isVolatileQualified()) {
    isQualified = true;
  }
  if (T.isRestrictQualified()) {
    isQualified = true;
  }

  if (T->isBuiltinType() || T->getTypeClass() == Type::Builtin) {
    if (isQualified) {
      rawname = registerBasicType(T);
      // XXX: temporary imcompletearray
      Node = createNode(T, "basicType", nullptr);
      basicTypeNodes.push_back(Node);
    } else {
      const Type *Tptr = T.getTypePtrOrNull();
      ASTContext &CXT = mangleContext->getASTContext();
      PrintingPolicy PP(CXT.getLangOpts());

      // XXX: temporary implementation
      rawname = static_cast<const BuiltinType*>(Tptr)->getName(PP).str();
      for (auto I = rawname.begin(); I != rawname.end(); ++I) {
        if (!std::isalnum(*I)) {
          *I = '_';
        }
      }
      mapFromQualTypeToName[T] = rawname;
    }
  } else switch (T->getTypeClass()) {
  case Type::Builtin:
    break;

  case Type::Complex:
    rawname = registerOtherType(T);
    // XXX: temporary implementation
    if (isQualified) {
      Node = createNode(T, "basicType", nullptr);
    } else {
      Node = createNode(T, "complexType", nullptr);
    }
    otherTypeNodes.push_back(Node);
    break;

  case Type::Pointer:
  case Type::BlockPointer:
  case Type::LValueReference:
  case Type::RValueReference:
  case Type::MemberPointer:
    {
      const PointerType *PT = dyn_cast<const PointerType>(T.getTypePtr());
      if (PT) {
        registerType(PT->getPointeeType(), nullptr, nullptr);
      }
      rawname = registerPointerType(T);
      Node = createNode(T, "pointerType", nullptr);
      if (PT) {
        xmlNewProp(Node, BAD_CAST "ref",
                   BAD_CAST getTypeName(PT->getPointeeType()).c_str());
      }
      pointerTypeNodes.push_back(Node);
    }
    break;

  case Type::ConstantArray:
    {
      const ConstantArrayType *CAT
        = dyn_cast<const ConstantArrayType>(T.getTypePtr());
      if (CAT) {
        registerType(CAT->getElementType(), nullptr, nullptr);
      }
      rawname = registerArrayType(T);
      Node = createNode(T, "arrayType", nullptr);
      if (CAT) {
        xmlNewProp(Node, BAD_CAST "element_type",
                   BAD_CAST getTypeName(CAT->getElementType()).c_str());
        xmlNewProp(Node, BAD_CAST "array_size",
                   BAD_CAST CAT->getSize().toString(10, false).c_str());
      }
      arrayTypeNodes.push_back(Node);
    }
    break;

  case Type::IncompleteArray:
  case Type::VariableArray:
  case Type::DependentSizedArray:
    {
      const ArrayType *AT = dyn_cast<const ArrayType>(T.getTypePtr());
      if (AT) {
        registerType(AT->getElementType(), nullptr, nullptr);
      }
      rawname = registerArrayType(T);
      Node = createNode(T, "arrayType", nullptr);
      if (AT){
        xmlNewProp(Node, BAD_CAST "element_type",
                   BAD_CAST getTypeName(AT->getElementType()).c_str());
      }
      arrayTypeNodes.push_back(Node);
    }
    break;

  case Type::DependentSizedExtVector:
  case Type::Vector:
  case Type::ExtVector:
    rawname = registerOtherType(T);
    // XXX: temporary implementation
    Node = createNode(T, "vectorType", nullptr);
    arrayTypeNodes.push_back(Node);
    break;

  case Type::FunctionProto:
  case Type::FunctionNoProto:
    {
      const FunctionType *FT = dyn_cast<const FunctionType>(T.getTypePtr());
      if (FT) {
        registerType(FT->getReturnType(), nullptr, nullptr);
      }
      rawname = registerFunctionType(T);
      Node = createNode(T, "functionType", nullptr);
      if (FT) {
        xmlNewProp(Node, BAD_CAST "return_type",
                   BAD_CAST getTypeName(FT->getReturnType()).c_str());
      }
      functionTypeNodes.push_back(Node);
    }
    break;

  case Type::UnresolvedUsing:
  case Type::Paren:
  case Type::Typedef:
    rawname = registerOtherType(T);
    // XXX: temporary implementation
    Node = createNode(T, "typedefType", nullptr);
    otherTypeNodes.push_back(Node);
    break;

  case Type::Adjusted:
  case Type::Decayed:
  case Type::TypeOfExpr:
  case Type::TypeOf:
  case Type::Decltype:
  case Type::UnaryTransform:
    rawname = registerOtherType(T);
    // XXX: temporary implementation
    Node = createNode(T, "otherType", nullptr);
    otherTypeNodes.push_back(Node);
    break;

  case Type::Record:
    rawname = registerRecordType(T);
    if (T->isStructureType()) {
      Node = createNode(T, "structType", nullptr);
      structTypeNodes.push_back(Node);
    } else if (T->isUnionType()) {
      Node = createNode(T, "unionType", nullptr);
      unionTypeNodes.push_back(Node);
    } else if (T->isClassType()) {
      // XXX: temporary implementation
      Node = createNode(T, "classType", nullptr);
      classTypeNodes.push_back(Node);
    } else {
      // XXX: temporary implementation
      Node = createNode(T, "unknownRecordType", nullptr);
      otherTypeNodes.push_back(Node);
    }
    break;

  case Type::Enum:
    rawname = registerEnumType(T);
    Node = createNode(T, "enumType", nullptr);
    enumTypeNodes.push_back(Node);
    break;

  case Type::Elaborated:
  case Type::Attributed:
  case Type::TemplateTypeParm:
  case Type::SubstTemplateTypeParm:
  case Type::SubstTemplateTypeParmPack:
  case Type::TemplateSpecialization:
  case Type::Auto:
  case Type::InjectedClassName:
  case Type::DependentName:
  case Type::DependentTemplateSpecialization:
  case Type::PackExpansion:
  case Type::ObjCObject:
  case Type::ObjCInterface:
  case Type::ObjCObjectPointer:
  case Type::Atomic:
    rawname = registerOtherType(T);
    // XXX: temporary implementation
    Node = createNode(T, "otherType", nullptr);
    otherTypeNodes.push_back(Node);
    break;
  }

  if (retNode != nullptr) {
    *retNode = Node;
  }
  mapFromNameToQualType[rawname] = T;
  mapFromQualTypeToXmlNodePtr[T] = Node;
}

void TypeTableInfo::registerLabelType(void)
{
  useLabelType = true;
}

std::string TypeTableInfo::getTypeName(QualType T)
{
  if (T.isNull()) {
    return "nullType";
  };

  const auto iter = mapFromQualTypeToName.find(T);
  std::string name;
  if (iter != mapFromQualTypeToName.end()) {
    name = iter->second;
  } else {
    registerType(T, nullptr, nullptr);
    name = mapFromQualTypeToName[T];
  }

  if (!map_is_already_set) {
    typenamemap["unsigned_int"] = "unsigned";
  }
  if (!OptTypeNameMap.empty()) {
    if (!map_is_already_set) {
      std::cerr << "use " << OptTypeNameMap << " as a typenamemap file" << std::endl;
      mapfile.open(OptTypeNameMap);
      if (mapfile.fail()) {
        std::cerr << OptTypeNameMap << ": cannot open" << std::endl;
        exit(1);
      }
      map_is_already_set = true;

      typenamemap.clear();
      std::string line;
      while (std::getline(mapfile, line)) {
        std::istringstream iss(line);
        std::string lhs, rhs;
        iss >> lhs >> rhs;
        if (!iss) {
          std::cerr << OptTypeNameMap << ": read error" << std::endl;
          exit(1);
        }
        typenamemap[lhs] = rhs;
        //std::cerr << "typenamemap: " << lhs << "->" << rhs << std::endl;
      }
    }
    std::string rhs = typenamemap[name];
    return !rhs.empty() ? rhs : name;
  }

  map_is_already_set = true;
  return name;
}

std::string TypeTableInfo::getTypeNameForLabel(void)
{
  if (!typenamemap["Label"].empty()) {
    return typenamemap["Label"];
  } else {
    return "Label";
  }
}

void TypeTableInfo::emitAllTypeNode(xmlNodePtr ParentNode)
{
  for (xmlNodePtr N : basicTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
  for (xmlNodePtr N : pointerTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
  if (useLabelType) {
    xmlNodePtr N = xmlNewNode(nullptr, BAD_CAST "pointerType");
    xmlNewProp(N, BAD_CAST "type", BAD_CAST getTypeNameForLabel().c_str());
    xmlNewProp(N, BAD_CAST "ref", BAD_CAST "void");
    xmlAddChild(ParentNode, N);
  }
  for (xmlNodePtr N : arrayTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
  for (xmlNodePtr N : structTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
  for (xmlNodePtr N : unionTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
  for (xmlNodePtr N : enumTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
  for (xmlNodePtr N : classTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
  for (xmlNodePtr N : functionTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
  for (xmlNodePtr N : otherTypeNodes) {
    xmlAddChild(ParentNode, N);
  }
}

std::vector<BaseClass> TypeTableInfo::getBaseClasses(clang::QualType type) {
  return inheritanceinfo->getInheritance(type);
}

void TypeTableInfo::addInheritance(clang::QualType derived, BaseClass base) {
  inheritanceinfo->addInheritance(derived, base);
}

bool TypeTableInfo::hasBaseClass(clang::QualType type) {
  return !( inheritanceinfo->getInheritance(type).empty() );
}

void TypeTableInfo::setNormalizability(clang::QualType T, bool b) {
  normalizability[T] = b;
}

bool TypeTableInfo::isNormalizable(clang::QualType T) {
  return normalizability[T];
}

void TypeTableInfo::dump() {
  for (auto& pair : mapFromNameToQualType) {
    auto name = pair.first;
    auto type = pair.second;
    std::cerr << name << ": " << type.getAsString() << std::endl;
  }
}

///
/// Local Variables:
/// indent-tabs-mode: nil
/// c-basic-offset: 2
/// End:
///
