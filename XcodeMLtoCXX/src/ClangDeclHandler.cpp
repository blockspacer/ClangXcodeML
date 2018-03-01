#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include "llvm/ADT/Optional.h"
#include "llvm/Support/Casting.h"

#include "LibXMLUtil.h"
#include "StringTree.h"
#include "XcodeMlNns.h"
#include "XMLString.h"

#include "XcodeMlName.h"

#include "XcodeMlType.h"
#include "XcodeMlUtil.h"
#include "XcodeMlEnvironment.h"

#include "AttrProc.h"
#include "NnsAnalyzer.h"
#include "SourceInfo.h"
#include "TypeAnalyzer.h"
#include "XMLWalker.h"

#include "CodeBuilder.h"

#include "ClangDeclHandler.h"

using XcodeMl::CodeFragment;
using CXXCodeGen::makeTokenNode;

#define DECLHANDLER_ARGS                                                      \
  xmlNodePtr node __attribute__((unused)),                                    \
      const CodeBuilder &w __attribute__((unused)),                           \
      SourceInfo &src __attribute__((unused))

#define DEFINE_DECLHANDLER(name) XcodeMl::CodeFragment name(DECLHANDLER_ARGS)

namespace {

std::vector<CodeFragment>
createNodes(xmlNodePtr node,
    const char *xpath,
    const CodeBuilder &w,
    SourceInfo &src) {
  std::vector<CodeFragment> vec;
  const auto targetNodes = findNodes(node, xpath, src.ctxt);
  for (auto &&targetNode : targetNodes) {
    vec.push_back(w.walk(targetNode, src));
  }
  return vec;
}

DEFINE_DECLHANDLER(callCodeBuilder) {
  return makeInnerNode(ProgramBuilder.walkChildren(node, src));
}

CodeFragment
makeTemplateHead(xmlNodePtr node, const CodeBuilder &w, SourceInfo &src) {
  const auto paramNodes =
      findNodes(node, "clangDecl[@class='TemplateTypeParm']", src.ctxt);
  std::vector<CXXCodeGen::StringTreeRef> params;
  for (auto &&paramNode : paramNodes) {
    params.push_back(w.walk(paramNode, src));
  }
  return makeTokenNode("template") + makeTokenNode("<") + join(",", params)
      + makeTokenNode(">");
}

DEFINE_DECLHANDLER(ClassTemplateProc) {
  if (const auto typeTableNode =
          findFirst(node, "xcodemlTypeTable", src.ctxt)) {
    src.typeTable = expandEnvironment(src.typeTable, typeTableNode, src.ctxt);
  }
  if (const auto nnsTableNode = findFirst(node, "xcodemlNnsTable", src.ctxt)) {
    src.nnsTable = expandNnsMap(src.nnsTable, nnsTableNode, src.ctxt);
  }
  const auto bodyNode =
      findFirst(node, "clangDecl[@class='CXXRecord']", src.ctxt);

  const auto head = makeTemplateHead(node, w, src);
  const auto body = w.walk(bodyNode, src);
  return head + body;
}

XcodeMl::CodeFragment
makeBases(const XcodeMl::ClassType &T, SourceInfo &src) {
  using namespace XcodeMl;
  const auto bases = T.getBases();
  std::vector<CodeFragment> decls;
  std::transform(bases.begin(),
      bases.end(),
      std::back_inserter(decls),
      [&src](ClassType::BaseClass base) {
        const auto T = src.typeTable.at(std::get<1>(base));
        const auto classT = llvm::cast<ClassType>(T.get());
        assert(classT);
        assert(classT->name().hasValue());
        return makeTokenNode(std::get<0>(base))
            + makeTokenNode(std::get<2>(base) ? "virtual" : "")
            + *(classT->name());
      });
  return decls.empty() ? CXXCodeGen::makeVoidNode()
                       : makeTokenNode(":") + CXXCodeGen::join(",", decls);
}

CodeFragment
emitClassDefinition(xmlNodePtr node,
    const CodeBuilder &w,
    SourceInfo &src,
    const XcodeMl::ClassType &classType) {
  if (isTrueProp(node, "is_implicit", false)) {
    return CXXCodeGen::makeVoidNode();
  }

  const auto memberNodes = findNodes(node, "clangDecl", src.ctxt);
  std::vector<XcodeMl::CodeFragment> decls;
  for (auto &&memberNode : memberNodes) {
    if (isTrueProp(memberNode, "is_implicit", false)) {
      continue;
    }
    /* Traverse `memberNode` regardless of whether `CodeBuilder` prints it. */
    const auto decl = w.walk(memberNode, src);

    const auto accessProp = getPropOrNull(memberNode, "access");
    if (accessProp.hasValue()) {
      const auto access = *accessProp;
      decls.push_back(makeTokenNode(access) + makeTokenNode(":") + decl);
    } else {
      decls.push_back(makeTokenNode(
          "\n/* Ignored a member with no access specifier */\n"));
    }
  }

  const auto classKey = makeTokenNode(getClassKey(classType.classKind()));
  const auto name = classType.isClassTemplateSpecialization()
      ? classType.getAsTemplateId(src.typeTable).getValue()
      : classType.name().getValue();

  return classKey + name + makeBases(classType, src) + makeTokenNode("{")
      + separateByBlankLines(decls) + makeTokenNode("}") + makeTokenNode(";")
      + CXXCodeGen::makeNewLineNode();
}

DEFINE_DECLHANDLER(ClassTemplatePartialSpecializationProc) {
  if (const auto typeTableNode =
          findFirst(node, "xcodemlTypeTable", src.ctxt)) {
    src.typeTable = expandEnvironment(src.typeTable, typeTableNode, src.ctxt);
  }
  if (const auto nnsTableNode = findFirst(node, "xcodemlNnsTable", src.ctxt)) {
    src.nnsTable = expandNnsMap(src.nnsTable, nnsTableNode, src.ctxt);
  }

  const auto T = src.typeTable.at(getType(node));
  const auto classT = llvm::cast<XcodeMl::ClassType>(T.get());
  const auto head = makeTemplateHead(node, w, src);
  if (isTrueProp(node, "is_this_declaration_a_definition", false)) {
    const auto def =
        emitClassDefinition(node, ClassDefinitionBuilder, src, *classT);
    return head + def;
  }
  /* forward declaration */
  const auto classKey = getClassKey(classT->classKind());
  const auto nameSpelling = classT->name().getValue();
  return head + makeTokenNode(classKey) + nameSpelling + makeTokenNode(";");
}

DEFINE_DECLHANDLER(ClassTemplateSpecializationProc) {
  const auto T = src.typeTable.at(getType(node));
  const auto classT = llvm::dyn_cast<XcodeMl::ClassType>(T.get());
  assert(classT && classT->name().hasValue());
  const auto nameSpelling = classT->name().getValue();

  const auto head =
      makeTokenNode("template") + makeTokenNode("<") + makeTokenNode(">");

  if (isTrueProp(node, "is_this_declaration_a_definition", false)) {
    return head
        + emitClassDefinition(node, ClassDefinitionBuilder, src, *classT);
  }

  /* forward declaration */
  const auto classKey = getClassKey(classT->classKind());
  return head + makeTokenNode(classKey) + nameSpelling + makeTokenNode(";");
}

void
setClassName(XcodeMl::ClassType &classType, SourceInfo &src) {
  if (classType.name().hasValue()) {
    return;
  }
  /* `classType` is unnamed.
   * Unnamed classes are problematic, so give a name to `classType`
   * such as `__xcodeml_1`.
   */
  classType.setName(src.getUniqueName());
}

DEFINE_DECLHANDLER(CXXRecordProc) {
  if (isTrueProp(node, "is_implicit", false)) {
    return CXXCodeGen::makeVoidNode();
  }

  const auto T = src.typeTable.at(getType(node));
  auto classT = llvm::dyn_cast<XcodeMl::ClassType>(T.get());
  assert(classT);

  setClassName(*classT, src);
  const auto nameSpelling = *(classT->name()); // now class name must exist

  if (isTrueProp(node, "is_this_declaration_a_definition", false)) {
    return emitClassDefinition(node, ClassDefinitionBuilder, src, *classT);
  }

  /* forward declaration */
  const auto classKey = getClassKey(classT->classKind());
  return makeTokenNode(classKey) + nameSpelling + makeTokenNode(";");
}

DEFINE_DECLHANDLER(emitInlineMemberFunction) {
  if (isTrueProp(node, "is_implicit", 0)) {
    return CXXCodeGen::makeVoidNode();
  }

  auto acc = CXXCodeGen::makeVoidNode();
  if (isTrueProp(node, "is_virtual", false)) {
    acc = acc + makeTokenNode("virtual");
  }
  if (isTrueProp(node, "is_static", false)) {
    acc = acc + makeTokenNode("static");
  }
  const auto paramNames = getParamNames(node, src);
  acc = acc + makeFunctionDeclHead(node, paramNames, src);

  if (const auto ctorInitList =
          findFirst(node, "constructorInitializerList", src.ctxt)) {
    acc = acc + ProgramBuilder.walk(ctorInitList, src);
  }

  if (const auto bodyNode = findFirst(node, "clangStmt", src.ctxt)) {
    const auto body = ProgramBuilder.walk(bodyNode, src);
    return acc + body;
  } else {
    return acc + makeTokenNode(";");
  }
  return acc;
}

DEFINE_DECLHANDLER(FieldDeclProc) {
  const auto nameNode = findFirst(node, "name", src.ctxt);
  const auto name = getUnqualIdFromNameNode(nameNode)->toString(src.typeTable);

  const auto dtident = getType(node);
  const auto T = src.typeTable.at(dtident);

  return makeDecl(T, name, src.typeTable) + makeTokenNode(";");
}

DEFINE_DECLHANDLER(FriendDeclProc) {
  if (auto TL = findFirst(node, "clangTypeLoc", src.ctxt)) {
    /* friend class declaration */
    const auto dtident = getType(TL);
    const auto T = src.typeTable.at(dtident);
    return makeTokenNode("friend")
        + makeDecl(T, CXXCodeGen::makeVoidNode(), src.typeTable)
        + makeTokenNode(";");
  }
  return makeTokenNode("friend") + callCodeBuilder(node, w, src);
}

DEFINE_DECLHANDLER(FunctionProc) {
  if (isTrueProp(node, "is_implicit", 0)) {
    return CXXCodeGen::makeVoidNode();
  }
  const auto type = getProp(node, "xcodemlType");
  const auto paramNames = getParamNames(node, src);
  auto acc = makeFunctionDeclHead(node, paramNames, src, true);

  if (const auto ctorInitList =
          findFirst(node, "constructorInitializerList", src.ctxt)) {
    acc = acc + w.walk(ctorInitList, src);
  }

  if (const auto bodyNode = findFirst(node, "clangStmt", src.ctxt)) {
    const auto body = w.walk(bodyNode, src);
    acc = acc + body;
  } else {
    acc = acc + makeTokenNode(";");
  }

  return wrapWithLangLink(acc, node, src);
}

DEFINE_DECLHANDLER(FunctionTemplateProc) {
  if (const auto typeTableNode =
          findFirst(node, "xcodemlTypeTable", src.ctxt)) {
    src.typeTable = expandEnvironment(src.typeTable, typeTableNode, src.ctxt);
  }
  if (const auto nnsTableNode = findFirst(node, "xcodemlNnsTable", src.ctxt)) {
    src.nnsTable = expandNnsMap(src.nnsTable, nnsTableNode, src.ctxt);
  }
  const auto paramNodes =
      findNodes(node, "clangDecl[@class='TemplateTypeParm']", src.ctxt);
  const auto body = findFirst(node, "clangDecl", src.ctxt);

  std::vector<CXXCodeGen::StringTreeRef> params;
  for (auto &&paramNode : paramNodes) {
    params.push_back(w.walk(paramNode, src));
  }

  return makeTokenNode("template") + makeTokenNode("<") + join(",", params)
      + makeTokenNode(">") + w.walk(body, src);
}

DEFINE_DECLHANDLER(LinkageSpecProc) {
  // We emit linkage specification by `wrapWithLangLink`
  // not here
  const auto decls = createNodes(node, "clangDecl", w, src);
  return insertNewLines(decls);
}

DEFINE_DECLHANDLER(NamespaceProc) {
  const auto nameNode = findFirst(node, "name", src.ctxt);
  const auto name = getUnqualIdFromNameNode(nameNode)->toString(src.typeTable);
  const auto head = makeTokenNode("namespace") + name;
  const auto decls = createNodes(node, "clangDecl", w, src);
  return head + wrapWithBrace(insertNewLines(decls));
}

void
setStructName(XcodeMl::Struct &s, xmlNodePtr node, SourceInfo &src) {
  const auto nameNode = findFirst(node, "name", src.ctxt);
  if (!nameNode || isEmpty(nameNode)) {
    s.setTagName(makeTokenNode(src.getUniqueName()));
    return;
  }
  const auto name = getUnqualIdFromNameNode(nameNode);
  const auto nameSpelling = name->toString(src.typeTable);
  s.setTagName(nameSpelling);
}

DEFINE_DECLHANDLER(RecordProc) {
  if (isTrueProp(node, "is_implicit", false)) {
    return CXXCodeGen::makeVoidNode();
  }
  const auto T = src.typeTable.at(getType(node));
  auto structT = llvm::dyn_cast<XcodeMl::Struct>(T.get());
  assert(structT);
  setStructName(*structT, node, src);
  const auto tagName = structT->tagName();

  const auto decls = createNodes(node, "clangDecl", w, src);
  return makeTokenNode("struct") + tagName
      + wrapWithBrace(insertNewLines(decls)) + makeTokenNode(";");
}

DEFINE_DECLHANDLER(TemplateTypeParmProc) {
  const auto name = getQualifiedName(node, src);
  const auto nameSpelling = name.toString(src.typeTable, src.nnsTable);

  const auto dtident = getType(node);
  auto T = src.typeTable.at(dtident);
  auto TTPT = llvm::cast<XcodeMl::TemplateTypeParm>(T.get());
  assert(TTPT);
  TTPT->setSpelling(nameSpelling);

  return makeTokenNode("typename") + nameSpelling;
}

DEFINE_DECLHANDLER(TranslationUnitProc) {
  if (const auto typeTableNode =
          findFirst(node, "xcodemlTypeTable", src.ctxt)) {
    src.typeTable = expandEnvironment(src.typeTable, typeTableNode, src.ctxt);
  }
  if (const auto nnsTableNode = findFirst(node, "xcodemlNnsTable", src.ctxt)) {
    src.nnsTable = expandNnsMap(src.nnsTable, nnsTableNode, src.ctxt);
  }
  const auto declNodes = findNodes(node, "clangDecl", src.ctxt);
  std::vector<CXXCodeGen::StringTreeRef> decls;
  for (auto &&declNode : declNodes) {
    decls.push_back(w.walk(declNode, src));
  }
  return separateByBlankLines(decls);
}

DEFINE_DECLHANDLER(TypedefProc) {
  if (isTrueProp(node, "is_implicit", 0)) {
    return CXXCodeGen::makeVoidNode();
  }
  const auto dtident = getProp(node, "xcodemlTypedefType");
  const auto T = src.typeTable.at(dtident);

  const auto nameNode = findFirst(node, "name", src.ctxt);
  const auto typedefName =
      getUnqualIdFromNameNode(nameNode)->toString(src.typeTable);

  return makeTokenNode("typedef") + makeDecl(T, typedefName, src.typeTable)
      + makeTokenNode(";");
}

CodeFragment
makeSpecifier(xmlNodePtr node) {
  const std::vector<std::tuple<std::string, std::string>> specifiers = {
      std::make_tuple("is_extern", "extern"),
      std::make_tuple("is_register", "register"),
      std::make_tuple("is_static", "static"),
      std::make_tuple("is_static_data_member", "static"),
      std::make_tuple("is_thread_local", "thread_local"),
  };
  auto code = CXXCodeGen::makeVoidNode();
  for (auto &&tuple : specifiers) {
    std::string attr, specifier;
    std::tie(attr, specifier) = tuple;
    if (isTrueProp(node, attr.c_str(), false)) {
      code = code + makeTokenNode(specifier);
    }
  }
  return code;
}

DEFINE_DECLHANDLER(VarProc) {
  const auto nameNode = findFirst(node, "name", src.ctxt);
  const auto name = getUnqualIdFromNameNode(nameNode)->toString(src.typeTable);
  const auto dtident = getProp(node, "xcodemlType");
  const auto T = src.typeTable.at(dtident);

  const auto decl = makeSpecifier(node) + makeDecl(T, name, src.typeTable);
  const auto initializerNode = findFirst(node, "clangStmt", src.ctxt);
  if (!initializerNode) {
    // does not have initalizer: `int x;`
    return makeDecl(T, name, src.typeTable) + makeTokenNode(";");
  }
  const auto astClass = getProp(initializerNode, "class");
  if (std::equal(astClass.begin(), astClass.end(), "CXXConstructExpr")) {
    // has initalizer and the variable is of class type
    const auto init = declareClassTypeInit(w, initializerNode, src);
    return wrapWithLangLink(decl + init + makeTokenNode(";"), node, src);
  }
  const auto init = w.walk(initializerNode, src);
  return decl + makeTokenNode("=") + init + makeTokenNode(";");
}

} // namespace

const ClangDeclHandlerType ClassDefinitionDeclHandler("class",
    CXXCodeGen::makeInnerNode,
    callCodeBuilder,
    {
        std::make_tuple("CXXMethod", emitInlineMemberFunction),
        std::make_tuple("CXXConstructor", emitInlineMemberFunction),
        std::make_tuple("CXXDestructor", emitInlineMemberFunction),
        std::make_tuple("CXXRecord", CXXRecordProc),
        std::make_tuple("Field", FieldDeclProc),
        std::make_tuple("Var", VarProc),
    });

const ClangDeclHandlerType ClangDeclHandler("class",
    CXXCodeGen::makeInnerNode,
    callCodeBuilder,
    {
        std::make_tuple("ClassTemplate", ClassTemplateProc),
        std::make_tuple(
            "ClassTemplateSpecialization", ClassTemplateSpecializationProc),
        std::make_tuple("ClassTemplatePartialSpecialization",
            ClassTemplatePartialSpecializationProc),
        std::make_tuple("CXXConstructor", FunctionProc),
        std::make_tuple("CXXMethod", FunctionProc),
        std::make_tuple("CXXRecord", CXXRecordProc),
        std::make_tuple("Field", FieldDeclProc),
        std::make_tuple("Friend", FriendDeclProc),
        std::make_tuple("Function", FunctionProc),
        std::make_tuple("FunctionTemplate", FunctionTemplateProc),
        std::make_tuple("LinkageSpec", LinkageSpecProc),
        std::make_tuple("Namespace", NamespaceProc),
        std::make_tuple("Record", RecordProc),
        std::make_tuple("TemplateTypeParm", TemplateTypeParmProc),
        std::make_tuple("TranslationUnit", TranslationUnitProc),
        std::make_tuple("Typedef", TypedefProc),
        std::make_tuple("Var", VarProc),
    });