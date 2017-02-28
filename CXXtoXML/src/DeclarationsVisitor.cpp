#include "XMLVisitorBase.h"
#include "TypeTableVisitor.h"
#include "DeclarationsVisitor.h"
#include "InheritanceInfo.h"
#include "clang/Basic/Builtins.h"
#include "clang/Lex/Lexer.h"
#include <map>
#include <sstream>
#include "OperationKinds.h"

using namespace clang;
using namespace llvm;

static cl::opt<bool>
OptTraceDeclarations("trace-declarations",
                     cl::desc("emit traces on <globalDeclarations>, <declarations>"),
                     cl::cat(CXX2XMLCategory));
static cl::opt<bool>
OptDisableDeclarations("disable-declarations",
                       cl::desc("disable <globalDeclarations>, <declarations>"),
                       cl::cat(CXX2XMLCategory));

const char *
DeclarationsVisitor::getVisitorName() const {
  return OptTraceDeclarations ? "Declarations" : nullptr;
}

static std::string
getSpelling(clang::Expr *E, const clang::ASTContext& CXT) {
  const unsigned INIT_BUFFER_SIZE = 32;
  SmallVector<char, INIT_BUFFER_SIZE> buffer;
  auto spelling = clang::Lexer::getSpelling(
      E->getExprLoc(),
      buffer,
      CXT.getSourceManager(),
      CXT.getLangOpts());
  return spelling.str();
}

static std::string
unsignedToHexString(unsigned u) {
  std::stringstream ss;
  ss << std::hex << "0x" << u;
  return ss.str();
}

bool
DeclarationsVisitor::PreVisitStmt(Stmt *S) {
  if (!S) {
    newComment("Stmt:NULL");
    return true;
  }

  newChild("clangStmt");
  newProp("class", S->getStmtClassName());
  setLocation(S->getLocStart());

  const BinaryOperator *BO = dyn_cast<const BinaryOperator>(S);
  if (BO) {
    auto namePtr = BOtoElemName(BO->getOpcode());
    if (namePtr) {
      newProp("binOpName", namePtr);
    } else {
      auto opName = BinaryOperator::getOpcodeStr(BO->getOpcode());
      newProp("clangBinOpToken", opName.str().c_str());
    }
  }
  const UnaryOperator *UO = dyn_cast<const UnaryOperator>(S);
  if (UO) {
    auto namePtr = UOtoElemName(UO->getOpcode());
    if (namePtr) {
      newProp("unaryOpName", namePtr);
    } else {
      auto opName = UnaryOperator::getOpcodeStr(UO->getOpcode());
      newProp("clangUnaryOpToken", opName.str().c_str());
    }
  }

  if (auto E = dyn_cast<clang::Expr>(S)) {
    newProp("valueCategory",
        E->isXValue() ? "xvalue" :
        E->isRValue() ? "prvalue": "lvalue");
    auto T = E->getType();
    newProp("xcodemlType", typetableinfo->getTypeName(T).c_str());
  }

  if (auto CE = dyn_cast<clang::CastExpr>(S)) {
    newProp("clangCastKind", CE->getCastKindName());
  }

  if (auto CL = dyn_cast<CharacterLiteral>(S)) {
    newProp("hexadecimalNotation",
        unsignedToHexString(CL->getValue()).c_str());
    newProp("token",
        getSpelling(CL, mangleContext->getASTContext()).c_str());
  }

  if (auto IL = dyn_cast<IntegerLiteral>(S)) {
    const unsigned INIT_BUFFER_SIZE = 32;
    SmallVector<char, INIT_BUFFER_SIZE> buffer;
    auto& CXT = mangleContext->getASTContext();
    auto spelling = clang::Lexer::getSpelling(
        IL->getLocation(),
        buffer,
        CXT.getSourceManager(),
        CXT.getLangOpts());
    newProp("token", spelling.str().c_str());
    std::string decimalNotation = IL->getValue().toString(10, true);
    newProp("decimalNotation", decimalNotation.c_str());
  }

  if (auto SL = dyn_cast<StringLiteral>(S)) {
    StringRef Data = SL->getString();
    std::string literalAsString;
    raw_string_ostream OS(literalAsString);

    for (unsigned i = 0, e = Data.size(); i != e; ++i) {
      unsigned char C = Data[i];
      if (C == '"' || C == '\\') {
        OS << '\\' << (char)C;
        continue;
      }
      if (isprint(C)) {
        OS << (char)C;
        continue;
      }
      switch (C) {
      case '\b': OS << "\\b"; break;
      case '\f': OS << "\\f"; break;
      case '\n': OS << "\\n"; break;
      case '\r': OS << "\\r"; break;
      case '\t': OS << "\\t"; break;
      default:
        OS << '\\';
        OS << ((C >> 6) & 0x7) + '0';
        OS << ((C >> 3) & 0x7) + '0';
        OS << ((C >> 0) & 0x7) + '0';
        break;
      }
    }
    OS.str();
    newProp("stringLiteral", literalAsString.c_str());
  }

  UnaryExprOrTypeTraitExpr *UEOTTE = dyn_cast<UnaryExprOrTypeTraitExpr>(S);
  if (UEOTTE) {
    //7.8 sizeof, alignof
    switch (UEOTTE->getKind()) {
    case UETT_SizeOf: {
      newChild("sizeOfExpr");
      TraverseType(static_cast<Expr*>(S)->getType());
      if (UEOTTE->isArgumentType()) {
        newChild("typeName");
        TraverseType(UEOTTE->getArgumentType());
      } else {
        TraverseStmt(UEOTTE->getArgumentExpr());
      }
      return true;
    }
    case UETT_AlignOf: {
      newChild("gccAlignOfExpr");
      TraverseType(static_cast<Expr*>(S)->getType());
      if (UEOTTE->isArgumentType()) {
        newChild("typeName");
        TraverseType(UEOTTE->getArgumentType());
      } else {
        TraverseStmt(UEOTTE->getArgumentExpr());
      }
      return true;

    }
    case UETT_VecStep:
      newChild("clangStmt");
      newProp("class", "UnaryExprOrTypeTraitExpr_UETT_VecStep");
      return true;

    //case UETT_OpenMPRequiredSimdAlign:
    //  NStmt("UnaryExprOrTypeTraitExpr(UETT_OpenMPRequiredSimdAlign");
    }
  }

  return true;
}

bool
DeclarationsVisitor::PreVisitType(QualType T) {
  if (T.isNull()) {
    newComment("Type:NULL");
    return true;
  }
  newProp("type", typetableinfo->getTypeName(T).c_str());
  return false;
}

bool
DeclarationsVisitor::PreVisitAttr(Attr *A) {
  if (!A) {
    newComment("Attr:NULL");
    return true;
  }
  newComment(std::string("Attr:") + A->getSpelling());
  newChild("gccAttribute");

  newProp("name", contentBySource(A->getLocation(), A->getLocation()).c_str());

  std::string prettyprint;
  raw_string_ostream OS(prettyprint);
  ASTContext &CXT = mangleContext->getASTContext();
  A->printPretty(OS, PrintingPolicy(CXT.getLangOpts()));
  newComment(OS.str());

  return true;
}

bool
DeclarationsVisitor::PreVisitDecl(Decl *D) {
  if (!D) {
    return true;
  }

  // default: use the AST name simply.
  newChild("clangDecl");
  newProp("class", D->getDeclKindName());
  setLocation(D->getLocation());

  auto &CXT = mangleContext->getASTContext();
  auto &SM = CXT.getSourceManager();
  if (auto RC = CXT.getRawCommentForDeclNoCache(D)) {
    auto comment = static_cast<std::string>(RC->getRawText(SM));
    addChild("comment", comment.c_str());
  } 

  if (D->isImplicit()) {
    newBoolProp("is_implicit", true);
  }
  if (D->getAccess() != AS_none) {
    newProp("access", AccessSpec(D->getAccess()).c_str());
  }

  NamedDecl *ND = dyn_cast<NamedDecl>(D);
  if (ND) {
    addChild("fullName", ND->getQualifiedNameAsString().c_str());
  }

  if (auto VD = dyn_cast<ValueDecl>(D)) {
    const auto T = VD->getType();
    newProp("xcodemlType", typetableinfo->getTypeName(T).c_str());
  }

  if (auto TD = dyn_cast<TypeDecl>(D)) {
    const auto T = QualType( TD->getTypeForDecl(), 0 );
    newProp("xcodemlType", typetableinfo->getTypeName(T).c_str());
  }

  if (auto VD = dyn_cast<VarDecl>(D)) {
    newBoolProp("has_init", VD->hasInit());
  }

  if (auto ND = dyn_cast<NamespaceDecl>(D)) {
    newBoolProp("is_inline", ND->isInline());
    newBoolProp("is_anonymous", ND->isAnonymousNamespace());
    if (! ND->isAnonymousNamespace()) {
      newBoolProp("is_first_declared", ND->isOriginalNamespace());
    }
  }
  if (auto FD = dyn_cast<FunctionDecl>(D)) {
    newBoolProp("is_defaulted", FD->isDefaulted());
    newBoolProp("is_deleted", FD->isDeletedAsWritten());
    newBoolProp("is_pure", FD->isPure());
    newBoolProp("is_variadic", FD->isVariadic());
  }
  if (auto MD = dyn_cast<CXXMethodDecl>(D)) {
    newBoolProp("is_const", MD->isConst());
    newBoolProp("is_static", MD->isStatic());
    newBoolProp("is_virtual", MD->isVirtual());
  }
  return true;
}

bool
DeclarationsVisitor::PreVisitDeclarationNameInfo(DeclarationNameInfo NI) {
  DeclarationName DN = NI.getName();
  IdentifierInfo *II = DN.getAsIdentifierInfo();

  newChild("clangDeclarationNameInfo",
          II ? II->getNameStart() : nullptr);
  newProp("class", NameForDeclarationName(DN));
  return true;
}

static std::string
SpecifierKindToString(
    clang::NestedNameSpecifier::SpecifierKind kind)
{
  switch (kind) {
    case NestedNameSpecifier::Identifier:
      return "identifier";
    case NestedNameSpecifier::Namespace:
      return "namespace";
    case NestedNameSpecifier::NamespaceAlias:
      return "namespace_alias";
    case NestedNameSpecifier::TypeSpec:
      return "type_specifier";
    case NestedNameSpecifier::TypeSpecWithTemplate:
      return "type_specifier_with_template";
    case NestedNameSpecifier::Global:
      return "global";
    case NestedNameSpecifier::Super:
      return "MS_super";
  }
}

static clang::IdentifierInfo*
getAsIdentifierInfo(clang::NestedNameSpecifier *NNS) {
  switch (NNS->getKind()) {
    case NestedNameSpecifier::Identifier:
      return NNS->getAsIdentifier();
    case NestedNameSpecifier::Namespace:
      return NNS->getAsNamespace()->getIdentifier();
    case NestedNameSpecifier::NamespaceAlias:
      return NNS->getAsNamespaceAlias()->getIdentifier();
    case NestedNameSpecifier::Super:
      return NNS->getAsRecordDecl()->getIdentifier();
    case NestedNameSpecifier::TypeSpec:
    case NestedNameSpecifier::TypeSpecWithTemplate:
    case NestedNameSpecifier::Global:
      return nullptr;
  }
}

bool
DeclarationsVisitor::PreVisitNestedNameSpecifierLoc(
    NestedNameSpecifierLoc N)
{
  if (auto NNS = N.getNestedNameSpecifier()) {
    newChild("clangNestedNameSpecifier");
    newProp("kind", SpecifierKindToString(NNS->getKind()).c_str());
    newProp("is_dependent", NNS->isDependent() ? "1":"0");
    newProp(
        "is_instantiation_dependent",
        NNS->isInstantiationDependent() ? "1":"0");
    if (auto ident = getAsIdentifierInfo(NNS)) {
      newProp(
          "name",
          ident->getNameStart());
    }
  }
  return true;
}
///
/// Local Variables:
/// indent-tabs-mode: nil
/// c-basic-offset: 2
/// End:
///
