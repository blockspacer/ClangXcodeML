#include "XMLVisitorBase.h"
#include "TypeTableVisitor.h"
#include "DeclarationsVisitor.h"
#include "InheritanceInfo.h"
#include "clang/Basic/Builtins.h"
#include "clang/Lex/Lexer.h"
#include <map>
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
    auto T = E->getType();
    newProp("xcodemlType", typetableinfo->getTypeName(T).c_str());
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
  return true;
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
  if (D->isImplicit()) {
    newBoolProp("is_implicit", true);
  }
  if (D->getAccess() != AS_none) {
    const auto getAccessStr =
      [](clang::AccessSpecifier access) -> std::string {
        switch (access) {
          case AS_public: return "public";
          case AS_private: return "private";
          case AS_protected: return "protected";
          case AS_none: abort();
        }
      };
    newProp("access", getAccessStr(D->getAccess()).c_str());
  }

  NamedDecl *ND = dyn_cast<NamedDecl>(D);
  if (ND) {
    addChild("fullName", ND->getQualifiedNameAsString().c_str());
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

///
/// Local Variables:
/// indent-tabs-mode: nil
/// c-basic-offset: 2
/// End:
///
