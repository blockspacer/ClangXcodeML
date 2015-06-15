#include "XcodeMlVisitorBase.h"
#include "SymbolsVisitor.h"
#include "DeclarationsVisitor.h"

using namespace clang;
using namespace llvm;

static cl::opt<bool>
OptTraceDeclarations("trace-declarations",
                     cl::desc("emit traces on <globalDeclarations>, <declarations>"),
                     cl::cat(C2XcodeMLCategory));
static cl::opt<bool>
OptDisableDeclarations("disable-declarations",
                       cl::desc("disable  <globalDeclarations>, <declarations>"),
                       cl::cat(C2XcodeMLCategory));

const char *
DeclarationsVisitor::getVisitorName() const {
  return OptTraceDeclarations ? "Declarations" : nullptr;
}

// helper macros

#define N(mes) do {newChild(mes); return true;} while (0)
#define NE(mes, content) do {                                           \
  if (!NowInExprStatement) {                                            \
    newChild("exprStatement");                                          \
    setLocation(static_cast<Expr*>(S)->getExprLoc());                   \
  }                                                                     \
  optContext.isInExprStatement = true;                                  \
  newChild(mes, content);                                               \
  return true;                                                          \
} while (0)

#define NC(mes, content) do {newChild(mes, content); return true;} while (0)
#define NT(mes) do {newChild(mes); TraverseType(T); return true;} while (0)
#define NS(mes) do {newChild(mes); if (!optContext.isInExprStatement) {setLocation(S->getLocStart());} return true;} while (0)

#define ND(mes) do {newChild(mes); setLocation(D->getLocation()); return true;} while (0)

bool
DeclarationsVisitor::PreVisitStmt(Stmt *S) {
  int NowInExprStatement = optContext.isInExprStatement;
  optContext.isInExprStatement = false;

  if (!S) {
    return false;
  }
  if (!optContext.sibling.empty()) {
    const char *name = optContext.sibling.back();
    if (name[0] == '+') {
      NowInExprStatement = true;
      newChild(name + 1);
    } else {
      newChild(name);
    }
    optContext.sibling.pop_back();
  }
  const BinaryOperator *BO = dyn_cast<const BinaryOperator>(S);
  if (BO) {
    if (!NowInExprStatement) {
      newChild("exprStatement");
      setLocation(BO->getExprLoc());
    }
    optContext.isInExprStatement = true;
    // XcodeML-C-0.9J.pdf: 7.6(assignExpr), 7.7, 7.10(commmaExpr)
    QualType T = BO->getType();
    switch (BO->getOpcode()) {
    case BO_PtrMemD:   NT("UNDEF_BO_PtrMemD");
    case BO_PtrMemI:   NT("UNDEF_BO_PtrMemI");
    case BO_Mul:       NT("mulExpr");
    case BO_Div:       NT("divExpr");
    case BO_Rem:       NT("modExpr");
    case BO_Add:       NT("plusExpr");
    case BO_Sub:       NT("minusExpr");
    case BO_Shl:       NT("LshiftExpr");
    case BO_Shr:       NT("RshiftExpr");
    case BO_LT:        NT("logLTExpr");
    case BO_GT:        NT("logGTExpr");
    case BO_LE:        NT("logLEExpr");
    case BO_GE:        NT("logGEExpr");
    case BO_EQ:        NT("logEQExpr");
    case BO_NE:        NT("logNEQExpr");
    case BO_And:       NT("bitAndExpr");
    case BO_Xor:       NT("bitXorExpr");
    case BO_Or:        NT("bitOrExpr");
    case BO_LAnd:      NT("logAndExpr");
    case BO_LOr:       NT("logOrExpr");
    case BO_Assign:    NT("assignExpr");
    case BO_Comma:     NT("commaExpr");
    case BO_MulAssign: NT("asgMulExpr");
    case BO_DivAssign: NT("asgDivExpr");
    case BO_RemAssign: NT("asgModExpr");
    case BO_AddAssign: NT("asgPlusExpr");
    case BO_SubAssign: NT("asgMinusExpr");
    case BO_ShlAssign: NT("asgLshiftExpr");
    case BO_ShrAssign: NT("asgRshiftExpr");
    case BO_AndAssign: NT("asgBitAndExpr");
    case BO_OrAssign:  NT("asgBitOrExpr");
    case BO_XorAssign: NT("asgBitXorExpr");
    }
  }
  const UnaryOperator *UO = dyn_cast<const UnaryOperator>(S);
  if (UO) {
    if (!NowInExprStatement) {
      newChild("exprStatement");
      setLocation(UO->getExprLoc());
    }
    optContext.isInExprStatement = true;
    // XcodeML-C-0.9J.pdf 7.2(varAddr), 7.3(pointerRef), 7.8, 7.11
    QualType T = UO->getType();
    switch (UO->getOpcode()) {
    case UO_PostInc:   NT("postIncrExpr");
    case UO_PostDec:   NT("postDecrExpr");
    case UO_PreInc:    NT("preIncrExpr");
    case UO_PreDec:    NT("preDecrExpr");
    case UO_AddrOf:    NT("varAddr");
    case UO_Deref:     NT("pointerRef");
    case UO_Plus:      NT("UNDEF_UO_Plus");
    case UO_Minus:     NT("unaryMinusExpr");
    case UO_Not:       NT("bitNotExpr");
    case UO_LNot:      NT("logNotExpr");
    case UO_Real:      NT("UNDEF_UO_Real");
    case UO_Imag:      NT("UNDEF_UO_Imag");
    case UO_Extension: NT("UNDEF_UO_Extension");
    }
  }

  switch (S->getStmtClass()) {
  case Stmt::NoStmtClass:     NS("Stmt_NoStmtClass");
  case Stmt::GCCAsmStmtClass: NS("Stmt_GCCAsmStmtClass");
  case Stmt::MSAsmStmtClass:  NS("Stmt_MSAsmStmtClass");
  case Stmt::AttributedStmtClass: NS("Stmt_AttributedStmtClass");
  case Stmt::BreakStmtClass: NS("breakStatement"); //6.7
  case Stmt::CXXCatchStmtClass: NS("Stmt_CXXCatchStmtClass");
  case Stmt::CXXForRangeStmtClass: NS("Stmt_CXXForRangeStmtClass");
  case Stmt::CXXTryStmtClass: NS("Stmt_CXXTryStmtClass");
  case Stmt::CapturedStmtClass: NS("Stmt_CapturedStmtClass");
  case Stmt::CompoundStmtClass: {
    // 6.2
    //SymbolsVisitor SV(astContext, curNode, "symbols", typetableinfo);
    //SV.TraverseStmt(S);
    optContext.children.push_back("body");
    NS("compoundStatement");
  }
  case Stmt::ContinueStmtClass:
    //6.8
    NS("continueStatement");
  case Stmt::DeclStmtClass: NS("Stmt_DeclStmtClass");
  case Stmt::DoStmtClass:
    //6.5
    optContext.children.push_back("+condition");    
    optContext.children.push_back("body");
    NS("doStatement");
  case Stmt::BinaryConditionalOperatorClass:
    //7.13
    NE("condExpr", nullptr);
  case Stmt::ConditionalOperatorClass:
    //7.13
    NE("condExpr", nullptr);
  case Stmt::AddrLabelExprClass: NS("Stmt_AddrLabelExprClass");
  case Stmt::ArraySubscriptExprClass: NS("Stmt_ArraySubscriptExprClass");
  case Stmt::ArrayTypeTraitExprClass: NS("Stmt_ArrayTypeTraitExprClass");
  case Stmt::AsTypeExprClass: NS("Stmt_AsTypeExprClass");
  case Stmt::AtomicExprClass: NS("Stmt_AtomicExprClass");
  case Stmt::BinaryOperatorClass: NS("Stmt_BinaryOperatorClass");
  case Stmt::CompoundAssignOperatorClass: NS("Stmt_CompoundAssignOperatorClass");
  case Stmt::BlockExprClass: NS("Stmt_BlockExprClass");
  case Stmt::CXXBindTemporaryExprClass: NS("Stmt_CXXBindTemporaryExprClass");
  case Stmt::CXXBoolLiteralExprClass: NS("Stmt_CXXBoolLiteralExprClass");
  case Stmt::CXXConstructExprClass: NS("Stmt_CXXConstructExprClass");
  case Stmt::CXXTemporaryObjectExprClass: NS("Stmt_CXXTemporaryObjectExprClass");
  case Stmt::CXXDefaultArgExprClass: NS("Stmt_CXXDefaultArgExprClass");
  case Stmt::CXXDefaultInitExprClass: NS("Stmt_CXXDefaultInitExprClass");
  case Stmt::CXXDeleteExprClass: NS("Stmt_CXXDeleteExprClass");
  case Stmt::CXXDependentScopeMemberExprClass: NS("Stmt_CXXDependentScopeMemberExprClass");
  case Stmt::CXXFoldExprClass: NS("Stmt_CXXFoldExprClass");
  case Stmt::CXXNewExprClass: NS("Stmt_CXXNewExprClass");
  case Stmt::CXXNoexceptExprClass: NS("Stmt_CXXNoexceptExprClass");
  case Stmt::CXXNullPtrLiteralExprClass: NS("Stmt_CXXNullPtrLiteralExprClass");
  case Stmt::CXXPseudoDestructorExprClass: NS("Stmt_CXXPseudoDestructorExprClass");
  case Stmt::CXXScalarValueInitExprClass: NS("Stmt_CXXScalarValueInitExprClass");
  case Stmt::CXXStdInitializerListExprClass: NS("Stmt_CXXStdInitializerListExprClass");
  case Stmt::CXXThisExprClass: NS("Stmt_CXXThisExprClass");
  case Stmt::CXXThrowExprClass: NS("Stmt_CXXThrowExprClass");
  case Stmt::CXXTypeidExprClass: NS("Stmt_CXXTypeidExprClass");
  case Stmt::CXXUnresolvedConstructExprClass: NS("Stmt_CXXUnresolvedConstructExprClass");
  case Stmt::CXXUuidofExprClass: NS("Stmt_CXXUuidofExprClass");
  case Stmt::CallExprClass: NS("functionCall"); //7.9 XXX
  case Stmt::CUDAKernelCallExprClass: NS("Stmt_CUDAKernelCallExprClass");
  case Stmt::CXXMemberCallExprClass: NS("Stmt_CXXMemberCallExprClass");
  case Stmt::CXXOperatorCallExprClass: NS("Stmt_CXXOperatorCallExprClass");
  case Stmt::UserDefinedLiteralClass: NS("Stmt_UserDefinedLiteralClass");
  case Stmt::CStyleCastExprClass: NS("castExpr"); //7.12
  case Stmt::CXXFunctionalCastExprClass: NS("Stmt_CXXFunctionalCastExprClass");
  case Stmt::CXXConstCastExprClass: NS("Stmt_CXXConstCastExprClass");
  case Stmt::CXXDynamicCastExprClass: NS("Stmt_CXXDynamicCastExprClass");
  case Stmt::CXXReinterpretCastExprClass: NS("Stmt_CXXReinterpretCastExprClass");
  case Stmt::CXXStaticCastExprClass: NS("Stmt_CXXStaticCastExprClass");
  case Stmt::ObjCBridgedCastExprClass: NS("Stmt_ObjCBridgedCastExprClass");
  case Stmt::ImplicitCastExprClass: NS("Stmt_ImplicitCastExprClass");
  case Stmt::CharacterLiteralClass: NS("Stmt_CharacterLiteralClass");
  case Stmt::ChooseExprClass: NS("Stmt_ChooseExprClass");
  case Stmt::CompoundLiteralExprClass: NS("Stmt_CompoundLiteralExprClass");
  case Stmt::ConvertVectorExprClass: NS("Stmt_ConvertVectorExprClass");
  case Stmt::DeclRefExprClass: NS("Stmt_DeclRefExprClass");
  case Stmt::DependentScopeDeclRefExprClass: NS("Stmt_DependentScopeDeclRefExprClass");
  case Stmt::DesignatedInitExprClass: NS("Stmt_DesignatedInitExprClass");
  case Stmt::ExprWithCleanupsClass: NS("Stmt_ExprWithCleanupsClass");
  case Stmt::ExpressionTraitExprClass: NS("Stmt_ExpressionTraitExprClass");
  case Stmt::ExtVectorElementExprClass: NS("Stmt_ExtVectorElementExprClass");
  case Stmt::FloatingLiteralClass: {
    //7.1
    double Value = static_cast<FloatingLiteral*>(S)->getValueAsApproximateDouble();
    raw_string_ostream OS(optContext.tmpstr);

    OS << Value;
    NE("floatConstant", OS.str().c_str());
  }
  case Stmt::FunctionParmPackExprClass: NS("Stmt_FunctionParmPackExprClass");
  case Stmt::GNUNullExprClass: NS("Stmt_GNUNullExprClass");
  case Stmt::GenericSelectionExprClass: NS("Stmt_GenericSelectionExprClass");
  case Stmt::ImaginaryLiteralClass: NS("Stmt_ImaginaryLiteralClass");
  case Stmt::ImplicitValueInitExprClass: NS("Stmt_ImplicitValueInitExprClass");
  case Stmt::InitListExprClass: NS("Stmt_InitListExprClass");
  case Stmt::IntegerLiteralClass: {
    //7.1 XXX: long long should be treated specially
    APInt Value = static_cast<IntegerLiteral*>(S)->getValue();
    raw_string_ostream OS(optContext.tmpstr);
    OS << *Value.getRawData();
    NE("intConstant", OS.str().c_str());
  }
  case Stmt::LambdaExprClass: NS("Stmt_LambdaExprClass");
  case Stmt::MSPropertyRefExprClass: NS("Stmt_MSPropertyRefExprClass");
  case Stmt::MaterializeTemporaryExprClass: NS("Stmt_MaterializeTemporaryExprClass");
  case Stmt::MemberExprClass:
    //7.5
    optContext.children.push_back("@member");
    NE("memberRef", nullptr); 
  case Stmt::ObjCArrayLiteralClass: NS("Stmt_ObjCArrayLiteralClass");
  case Stmt::ObjCBoolLiteralExprClass: NS("Stmt_ObjCBoolLiteralExprClass");
  case Stmt::ObjCBoxedExprClass: NS("Stmt_ObjCBoxedExprClass");
  case Stmt::ObjCDictionaryLiteralClass: NS("Stmt_ObjCDictionaryLiteralClass");
  case Stmt::ObjCEncodeExprClass: NS("Stmt_ObjCEncodeExprClass");
  case Stmt::ObjCIndirectCopyRestoreExprClass: NS("Stmt_ObjCIndirectCopyRestoreExprClass");
  case Stmt::ObjCIsaExprClass: NS("Stmt_ObjCIsaExprClass");
  case Stmt::ObjCIvarRefExprClass: NS("Stmt_ObjCIvarRefExprClass");
  case Stmt::ObjCMessageExprClass: NS("Stmt_ObjCMessageExprClass");
  case Stmt::ObjCPropertyRefExprClass: NS("Stmt_ObjCPropertyRefExprClass");
  case Stmt::ObjCProtocolExprClass: NS("Stmt_ObjCProtocolExprClass");
  case Stmt::ObjCSelectorExprClass: NS("Stmt_ObjCSelectorExprClass");
  case Stmt::ObjCStringLiteralClass: NS("Stmt_ObjCStringLiteralClass");
  case Stmt::ObjCSubscriptRefExprClass: NS("Stmt_ObjCSubscriptRefExprClass");
  case Stmt::OffsetOfExprClass: NS("Stmt_OffsetOfExprClass");
  case Stmt::OpaqueValueExprClass: NS("Stmt_OpaqueValueExprClass");
  case Stmt::UnresolvedLookupExprClass: NS("Stmt_UnresolvedLookupExprClass");
  case Stmt::UnresolvedMemberExprClass: NS("Stmt_UnresolvedMemberExprClass");
  case Stmt::PackExpansionExprClass: NS("Stmt_PackExpansionExprClass");
  case Stmt::ParenExprClass: return true;; // no explicit node
  case Stmt::ParenListExprClass: NS("Stmt_ParenListExprClass");
  case Stmt::PredefinedExprClass: NS("Stmt_PredefinedExprClass");
  case Stmt::PseudoObjectExprClass: NS("Stmt_PseudoObjectExprClass");
  case Stmt::ShuffleVectorExprClass: NS("Stmt_ShuffleVectorExprClass");
  case Stmt::SizeOfPackExprClass: NS("Stmt_SizeOfPackExprClass");
  case Stmt::StmtExprClass: NS("Stmt_StmtExprClass");
  case Stmt::StringLiteralClass: {
    //7.1
    StringRef Data = static_cast<StringLiteral*>(S)->getString();
    raw_string_ostream OS(optContext.tmpstr);

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
    NE("stringConstant", OS.str().c_str());
  }
  case Stmt::SubstNonTypeTemplateParmExprClass: NS("Stmt_SubstNonTypeTemplateParmExprClass");
  case Stmt::SubstNonTypeTemplateParmPackExprClass: NS("Stmt_SubstNonTypeTemplateParmPackExprClass");
  case Stmt::TypeTraitExprClass: NS("Stmt_TypeTraitExprClass");
  case Stmt::TypoExprClass: NS("Stmt_TypoExprClass");
  case Stmt::UnaryExprOrTypeTraitExprClass: NS("Stmt_UnaryExprOrTypeTraitExprClass");
  case Stmt::UnaryOperatorClass: NS("Stmt_UnaryOperatorClass");
  case Stmt::VAArgExprClass: NS("Stmt_VAArgExprClass");
  case Stmt::ForStmtClass:
    //6.6
    optContext.children.push_back("body");
    optContext.children.push_back("iter");
    optContext.children.push_back("+condition");
    optContext.children.push_back("init");
    NS("forStatement");
  case Stmt::GotoStmtClass: NS("gotoStatement"); //6.10 XXX
  case Stmt::IfStmtClass:
    //6.3
    optContext.children.push_back("else");
    optContext.children.push_back("then");
    optContext.children.push_back("+condition");
    NS("ifStatement");
  case Stmt::IndirectGotoStmtClass: NS("Stmt_IndirectGotoStmtClass");
  case Stmt::LabelStmtClass:
    //6.11
    optContext.children.push_back("name");
    NS("statementLabel");
  case Stmt::MSDependentExistsStmtClass: NS("Stmt_MSDependentExistsStmtClass");
  case Stmt::NullStmtClass: NS("Stmt_NullStmtClass");
  case Stmt::OMPAtomicDirectiveClass: NS("Stmt_OMPAtomicDirectiveClass");
  case Stmt::OMPBarrierDirectiveClass: NS("Stmt_OMPBarrierDirectiveClass");
  case Stmt::OMPCriticalDirectiveClass: NS("Stmt_OMPCriticalDirectiveClass");
  case Stmt::OMPFlushDirectiveClass: NS("Stmt_OMPFlushDirectiveClass");
  case Stmt::OMPForDirectiveClass: NS("Stmt_OMPForDirectiveClass");
  case Stmt::OMPForSimdDirectiveClass: NS("Stmt_OMPForSimdDirectiveClass");
  case Stmt::OMPParallelForDirectiveClass: NS("Stmt_OMPParallelForDirectiveClass");
  case Stmt::OMPParallelForSimdDirectiveClass: NS("Stmt_OMPParallelForSimdDirectiveClass");
  case Stmt::OMPSimdDirectiveClass: NS("Stmt_OMPSimdDirectiveClass");
  case Stmt::OMPMasterDirectiveClass: NS("Stmt_OMPMasterDirectiveClass");
  case Stmt::OMPOrderedDirectiveClass: NS("Stmt_OMPOrderedDirectiveClass");
  case Stmt::OMPParallelDirectiveClass: NS("Stmt_OMPParallelDirectiveClass");
  case Stmt::OMPParallelSectionsDirectiveClass: NS("Stmt_OMPParallelSectionsDirectiveClass");
  case Stmt::OMPSectionDirectiveClass: NS("Stmt_OMPSectionDirectiveClass");
  case Stmt::OMPSectionsDirectiveClass: NS("Stmt_OMPSectionsDirectiveClass");
  case Stmt::OMPSingleDirectiveClass: NS("Stmt_OMPSingleDirectiveClass");
  case Stmt::OMPTargetDirectiveClass: NS("Stmt_OMPTargetDirectiveClass");
  case Stmt::OMPTaskDirectiveClass: NS("Stmt_OMPTaskDirectiveClass");
  case Stmt::OMPTaskwaitDirectiveClass: NS("Stmt_OMPTaskwaitDirectiveClass");
  case Stmt::OMPTaskyieldDirectiveClass: NS("Stmt_OMPTaskyieldDirectiveClass");
  case Stmt::OMPTeamsDirectiveClass: NS("Stmt_OMPTeamsDirectiveClass");
  case Stmt::ObjCAtCatchStmtClass: NS("Stmt_ObjCAtCatchStmtClass");
  case Stmt::ObjCAtFinallyStmtClass: NS("Stmt_ObjCAtFinallyStmtClass");
  case Stmt::ObjCAtSynchronizedStmtClass: NS("Stmt_ObjCAtSynchronizedStmtClass");
  case Stmt::ObjCAtThrowStmtClass: NS("Stmt_ObjCAtThrowStmtClass");
  case Stmt::ObjCAtTryStmtClass: NS("Stmt_ObjCAtTryStmtClass");
  case Stmt::ObjCAutoreleasePoolStmtClass: NS("Stmt_ObjCAutoreleasePoolStmtClass");
  case Stmt::ObjCForCollectionStmtClass: NS("Stmt_ObjCForCollectionStmtClass");
  case Stmt::ReturnStmtClass:
    //6.9
    optContext.isInExprStatement = true;
    NS("returnStatement");
  case Stmt::SEHExceptStmtClass: NS("Stmt_SEHExceptStmtClass");
  case Stmt::SEHFinallyStmtClass: NS("Stmt_SEHFinallyStmtClass");
  case Stmt::SEHLeaveStmtClass: NS("Stmt_SEHLeaveStmtClass");
  case Stmt::SEHTryStmtClass: NS("Stmt_SEHTryStmtClass");
  case Stmt::CaseStmtClass:
    //6.13
    optContext.children.push_back("value");
    NS("caseLabel");
  case Stmt::DefaultStmtClass:
    //6.15
    NS("defaultLabel");
  case Stmt::SwitchStmtClass:
    //6.12
    optContext.children.push_back("body");
    optContext.children.push_back("value");
    NS("switchStatement");
  case Stmt::WhileStmtClass:
    //6.4
    optContext.children.push_back("body");
    optContext.children.push_back("+condition");    
    NS("whileStatement");
  }
}

#if 0
bool
DeclarationsVisitor::PreVisitStmt(Stmt *S)
{
  if (!S) {
    return true;
  }
  const BinaryOperator *BO = dyn_cast<const BinaryOperator>(S);
  if (BO) {
    TraverseType(BO->getType());
    return true;
  }
  const UnaryOperator *UO = dyn_cast<const UnaryOperator>(S);
  if (UO) {
    TraverseType(UO->getType());
    return true;
  }
  if (!optContext.isInExprStatement) {
    setLocation(S->getLocStart());
  }
  return true;
}

bool
DeclarationsVisitor::PostVisitStmt(Stmt *S)
{
  if (!S) {
    return true;
  }
  switch (S->getStmtClass()) {
  case Stmt::MemberExprClass:
    xmlNodePtr memberNode = ...;
    xmlUnlinkNode(memberNode);
    setProp("member", xmlNodeGetContent(memberNode));
    xmlFreeNode(memberNode);
    break;

  default:
    return true;
  }
}
#endif

bool
DeclarationsVisitor::PreVisitType(QualType T) {
  if (T.isNull()) {
    N("Type_NULL");
  }
  switch (T->getTypeClass()) {
  case Type::Builtin: N("Type_Builtin");
  case Type::Complex: N("Type_Complex");
  case Type::Pointer: N("Type_Pointer");
  case Type::BlockPointer: N("Type_BlockPointer");
  case Type::LValueReference: N("Type_LValueReference");
  case Type::RValueReference: N("Type_RValueReference");
  case Type::MemberPointer: N("Type_MemberPointer");
  case Type::ConstantArray: N("Type_ConstantArray");
  case Type::IncompleteArray: N("Type_IncompleteArray");
  case Type::VariableArray: N("Type_VariableArray");
  case Type::DependentSizedArray: N("Type_DependentSizedArray");
  case Type::DependentSizedExtVector: N("Type_DependentSizedExtVector");
  case Type::Vector: N("Type_Vector");
  case Type::ExtVector: N("Type_ExtVector");
  case Type::FunctionProto: N("Type_FunctionProto");
  case Type::FunctionNoProto: N("Type_FunctionNoProto");
  case Type::UnresolvedUsing: N("Type_UnresolvedUsing");
  case Type::Paren: N("Type_Paren");
  case Type::Typedef: N("Type_Typedef");
  case Type::Adjusted: N("Type_Adjusted");
  case Type::Decayed: N("Type_Decayed");
  case Type::TypeOfExpr: N("Type_TypeOfExpr");
  case Type::TypeOf: N("Type_TypeOf");
  case Type::Decltype: N("Type_Decltype");
  case Type::UnaryTransform: N("Type_UnaryTransform");
  case Type::Record: N("Type_Record");
  case Type::Enum: N("Type_Enum");
  case Type::Elaborated: N("Type_Elaborated");
  case Type::Attributed: N("Type_Attributed");
  case Type::TemplateTypeParm: N("Type_TemplateTypeParm");
  case Type::SubstTemplateTypeParm: N("Type_SubstTemplateTypeParm");
  case Type::SubstTemplateTypeParmPack: N("Type_SubstTemplateTypeParmPack");
  case Type::TemplateSpecialization: N("Type_TemplateSpecialization");
  case Type::Auto: N("Type_Auto");
  case Type::InjectedClassName: N("Type_InjectedClassName");
  case Type::DependentName: N("Type_DependentName");
  case Type::DependentTemplateSpecialization: N("Type_DependentTemplateSpecialization");
  case Type::PackExpansion: N("Type_PackExpansion");
  case Type::ObjCObject: N("Type_ObjCObject");
  case Type::ObjCInterface: N("Type_ObjCInterface");
  case Type::ObjCObjectPointer: N("Type_ObjCObjectPointer");
  case Type::Atomic: N("Type_Atomic");
  }
}

bool
DeclarationsVisitor::PreVisitTypeLoc(TypeLoc TL) {
  if (TL.isNull()) {
    N("TypeLoc_NULL");
  }
  switch (TL.getTypeLocClass()) {
  case TypeLoc::Qualified: N("TypeLoc_Qualified");
  case TypeLoc::Builtin: N("TypeLoc_Builtin");
  case TypeLoc::Complex: N("TypeLoc_Complex");
  case TypeLoc::Pointer: N("TypeLoc_Pointer");
  case TypeLoc::BlockPointer: N("TypeLoc_BlockPointer");
  case TypeLoc::LValueReference: N("TypeLoc_LValueReference");
  case TypeLoc::RValueReference: N("TypeLoc_RValueReference");
  case TypeLoc::MemberPointer: N("TypeLoc_MemberPointer");
  case TypeLoc::ConstantArray: N("TypeLoc_ConstantArray");
  case TypeLoc::IncompleteArray: N("TypeLoc_IncompleteArray");
  case TypeLoc::VariableArray: N("TypeLoc_VariableArray");
  case TypeLoc::DependentSizedArray: N("TypeLoc_DependentSizedArray");
  case TypeLoc::DependentSizedExtVector: N("TypeLoc_DependentSizedExtVector");
  case TypeLoc::Vector: N("TypeLoc_Vector");
  case TypeLoc::ExtVector: N("TypeLoc_ExtVector");
  case TypeLoc::FunctionProto: N("TypeLoc_FunctionProto");
  case TypeLoc::FunctionNoProto: N("TypeLoc_FunctionNoProto");
  case TypeLoc::UnresolvedUsing: N("TypeLoc_UnresolvedUsing");
  case TypeLoc::Paren: N("TypeLoc_Paren");
  case TypeLoc::Typedef: N("TypeLoc_Typedef");
  case TypeLoc::Adjusted: N("TypeLoc_Adjusted");
  case TypeLoc::Decayed: N("TypeLoc_Decayed");
  case TypeLoc::TypeOfExpr: N("TypeLoc_TypeOfExpr");
  case TypeLoc::TypeOf: N("TypeLoc_TypeOf");
  case TypeLoc::Decltype: N("TypeLoc_Decltype");
  case TypeLoc::UnaryTransform: N("TypeLoc_UnaryTransform");
  case TypeLoc::Record: N("TypeLoc_Record");
  case TypeLoc::Enum: N("TypeLoc_Enum");
  case TypeLoc::Elaborated: N("TypeLoc_Elaborated");
  case TypeLoc::Attributed: N("TypeLoc_Attributed");
  case TypeLoc::TemplateTypeParm: N("TypeLoc_TemplateTypeParm");
  case TypeLoc::SubstTemplateTypeParm: N("TypeLoc_SubstTemplateTypeParm");
  case TypeLoc::SubstTemplateTypeParmPack: N("TypeLoc_SubstTemplateTypeParmPack");
  case TypeLoc::TemplateSpecialization: N("TypeLoc_TemplateSpecialization");
  case TypeLoc::Auto: N("TypeLoc_Auto");
  case TypeLoc::InjectedClassName: N("TypeLoc_InjectedClassName");
  case TypeLoc::DependentName: N("TypeLoc_DependentName");
  case TypeLoc::DependentTemplateSpecialization: N("TypeLoc_DependentTemplateSpecialization");
  case TypeLoc::PackExpansion: N("TypeLoc_PackExpansion");
  case TypeLoc::ObjCObject: N("TypeLoc_ObjCObject");
  case TypeLoc::ObjCInterface: N("TypeLoc_ObjCInterface");
  case TypeLoc::ObjCObjectPointer: N("TypeLoc_ObjCObjectPointer");
  case TypeLoc::Atomic: N("TypeLoc_Atomic");
  }
}

bool
DeclarationsVisitor::PreVisitAttr(Attr *A) {
  if (!A) {
    N("Attr_NULL");
  }
  switch (A->getKind()) {
  case attr::NUM_ATTRS: N("Attr_NUMATTRS"); // may not be used
  case attr::AMDGPUNumSGPR: N("Attr_AMDGPUNumSGPR");
  case attr::AMDGPUNumVGPR: N("Attr_AMDGPUNumVGPR");
  case attr::ARMInterrupt: N("Attr_ARMInterrupt");
  case attr::AcquireCapability: N("Attr_AcquireCapability");
  case attr::AcquiredAfter: N("Attr_AcquiredAfter");
  case attr::AcquiredBefore: N("Attr_AcquiredBefore");
  case attr::Alias: N("Attr_Alias");
  case attr::AlignMac68k: N("Attr_AlignMac68k");
  case attr::AlignValue: N("Attr_AlignValue");
  case attr::Aligned: N("Attr_Aligned");
  case attr::AlwaysInline: N("Attr_AlwaysInline");
  case attr::AnalyzerNoReturn: N("Attr_AnalyzerNoReturn");
  case attr::Annotate: N("Attr_Annotate");
  case attr::ArcWeakrefUnavailable: N("Attr_ArcWeakrefUnavailable");
  case attr::ArgumentWithTypeTag: N("Attr_ArgumentWithTypeTag");
  case attr::AsmLabel: N("Attr_AsmLabel");
  case attr::AssertCapability: N("Attr_AssertCapability");
  case attr::AssertExclusiveLock: N("Attr_AssertExclusiveLock");
  case attr::AssertSharedLock: N("Attr_AssertSharedLock");
  case attr::AssumeAligned: N("Attr_AssumeAligned");
  case attr::Availability: N("Attr_Availability");
  case attr::Blocks: N("Attr_Blocks");
  case attr::C11NoReturn: N("Attr_C11NoReturn");
  case attr::CDecl: N("Attr_CDecl");
  case attr::CFAuditedTransfer: N("Attr_CFAuditedTransfer");
  case attr::CFConsumed: N("Attr_CFConsumed");
  case attr::CFReturnsNotRetained: N("Attr_CFReturnsNotRetained");
  case attr::CFReturnsRetained: N("Attr_CFReturnsRetained");
  case attr::CFUnknownTransfer: N("Attr_CFUnknownTransfer");
  case attr::CUDAConstant: N("Attr_CUDAConstant");
  case attr::CUDADevice: N("Attr_CUDADevice");
  case attr::CUDAGlobal: N("Attr_CUDAGlobal");
  case attr::CUDAHost: N("Attr_CUDAHost");
  case attr::CUDAInvalidTarget: N("Attr_CUDAInvalidTarget");
  case attr::CUDALaunchBounds: N("Attr_CUDALaunchBounds");
  case attr::CUDAShared: N("Attr_CUDAShared");
  case attr::CXX11NoReturn: N("Attr_CXX11NoReturn");
  case attr::CallableWhen: N("Attr_CallableWhen");
  case attr::Capability: N("Attr_Capability");
  case attr::CapturedRecord: N("Attr_CapturedRecord");
  case attr::CarriesDependency: N("Attr_CarriesDependency");
  case attr::Cleanup: N("Attr_Cleanup");
  case attr::Cold: N("Attr_Cold");
  case attr::Common: N("Attr_Common");
  case attr::Const: N("Attr_Const");
  case attr::Constructor: N("Attr_Constructor");
  case attr::Consumable: N("Attr_Consumable");
  case attr::ConsumableAutoCast: N("Attr_ConsumableAutoCast");
  case attr::ConsumableSetOnRead: N("Attr_ConsumableSetOnRead");
  case attr::DLLExport: N("Attr_DLLExport");
  case attr::DLLImport: N("Attr_DLLImport");
  case attr::Deprecated: N("Attr_Deprecated");
  case attr::Destructor: N("Attr_Destructor");
  case attr::EnableIf: N("Attr_EnableIf");
  case attr::ExclusiveTrylockFunction: N("Attr_ExclusiveTrylockFunction");
  case attr::FallThrough: N("Attr_FallThrough");
  case attr::FastCall: N("Attr_FastCall");
  case attr::Final: N("Attr_Final");
  case attr::Flatten: N("Attr_Flatten");
  case attr::Format: N("Attr_Format");
  case attr::FormatArg: N("Attr_FormatArg");
  case attr::GNUInline: N("Attr_GNUInline");
  case attr::GuardedBy: N("Attr_GuardedBy");
  case attr::GuardedVar: N("Attr_GuardedVar");
  case attr::Hot: N("Attr_Hot");
  case attr::IBAction: N("Attr_IBAction");
  case attr::IBOutlet: N("Attr_IBOutlet");
  case attr::IBOutletCollection: N("Attr_IBOutletCollection");
  case attr::InitPriority: N("Attr_InitPriority");
  case attr::InitSeg: N("Attr_InitSeg");
  case attr::IntelOclBicc: N("Attr_IntelOclBicc");
  case attr::LockReturned: N("Attr_LockReturned");
  case attr::LocksExcluded: N("Attr_LocksExcluded");
  case attr::LoopHint: N("Attr_LoopHint");
  case attr::MSABI: N("Attr_MSABI");
  case attr::MSInheritance: N("Attr_MSInheritance");
  case attr::MSP430Interrupt: N("Attr_MSP430Interrupt");
  case attr::MSVtorDisp: N("Attr_MSVtorDisp");
  case attr::Malloc: N("Attr_Malloc");
  case attr::MaxFieldAlignment: N("Attr_MaxFieldAlignment");
  case attr::MayAlias: N("Attr_MayAlias");
  case attr::MinSize: N("Attr_MinSize");
  case attr::Mips16: N("Attr_Mips16");
  case attr::Mode: N("Attr_Mode");
  case attr::MsStruct: N("Attr_MsStruct");
  case attr::NSConsumed: N("Attr_NSConsumed");
  case attr::NSConsumesSelf: N("Attr_NSConsumesSelf");
  case attr::NSReturnsAutoreleased: N("Attr_NSReturnsAutoreleased");
  case attr::NSReturnsNotRetained: N("Attr_NSReturnsNotRetained");
  case attr::NSReturnsRetained: N("Attr_NSReturnsRetained");
  case attr::Naked: N("Attr_Naked");
  case attr::NoCommon: N("Attr_NoCommon");
  case attr::NoDebug: N("Attr_NoDebug");
  case attr::NoDuplicate: N("Attr_NoDuplicate");
  case attr::NoInline: N("Attr_NoInline");
  case attr::NoInstrumentFunction: N("Attr_NoInstrumentFunction");
  case attr::NoMips16: N("Attr_NoMips16");
  case attr::NoReturn: N("Attr_NoReturn");
  case attr::NoSanitizeAddress: N("Attr_NoSanitizeAddress");
  case attr::NoSanitizeMemory: N("Attr_NoSanitizeMemory");
  case attr::NoSanitizeThread: N("Attr_NoSanitizeThread");
  case attr::NoSplitStack: N("Attr_NoSplitStack");
  case attr::NoThreadSafetyAnalysis: N("Attr_NoThreadSafetyAnalysis");
  case attr::NoThrow: N("Attr_NoThrow");
  case attr::NonNull: N("Attr_NonNull");
  case attr::OMPThreadPrivateDecl: N("Attr_OMPThreadPrivateDecl");
  case attr::ObjCBridge: N("Attr_ObjCBridge");
  case attr::ObjCBridgeMutable: N("Attr_ObjCBridgeMutable");
  case attr::ObjCBridgeRelated: N("Attr_ObjCBridgeRelated");
  case attr::ObjCDesignatedInitializer: N("Attr_ObjCDesignatedInitializer");
  case attr::ObjCException: N("Attr_ObjCException");
  case attr::ObjCExplicitProtocolImpl: N("Attr_ObjCExplicitProtocolImpl");
  case attr::ObjCMethodFamily: N("Attr_ObjCMethodFamily");
  case attr::ObjCNSObject: N("Attr_ObjCNSObject");
  case attr::ObjCPreciseLifetime: N("Attr_ObjCPreciseLifetime");
  case attr::ObjCRequiresPropertyDefs: N("Attr_ObjCRequiresPropertyDefs");
  case attr::ObjCRequiresSuper: N("Attr_ObjCRequiresSuper");
  case attr::ObjCReturnsInnerPointer: N("Attr_ObjCReturnsInnerPointer");
  case attr::ObjCRootClass: N("Attr_ObjCRootClass");
  case attr::ObjCRuntimeName: N("Attr_ObjCRuntimeName");
  case attr::OpenCLImageAccess: N("Attr_OpenCLImageAccess");
  case attr::OpenCLKernel: N("Attr_OpenCLKernel");
  case attr::OptimizeNone: N("Attr_OptimizeNone");
  case attr::Overloadable: N("Attr_Overloadable");
  case attr::Override: N("Attr_Override");
  case attr::Ownership: N("Attr_Ownership");
  case attr::Packed: N("Attr_Packed");
  case attr::ParamTypestate: N("Attr_ParamTypestate");
  case attr::Pascal: N("Attr_Pascal");
  case attr::Pcs: N("Attr_Pcs");
  case attr::PnaclCall: N("Attr_PnaclCall");
  case attr::PtGuardedBy: N("Attr_PtGuardedBy");
  case attr::PtGuardedVar: N("Attr_PtGuardedVar");
  case attr::Pure: N("Attr_Pure");
  case attr::ReleaseCapability: N("Attr_ReleaseCapability");
  case attr::ReqdWorkGroupSize: N("Attr_ReqdWorkGroupSize");
  case attr::RequiresCapability: N("Attr_RequiresCapability");
  case attr::ReturnTypestate: N("Attr_ReturnTypestate");
  case attr::ReturnsNonNull: N("Attr_ReturnsNonNull");
  case attr::ReturnsTwice: N("Attr_ReturnsTwice");
  case attr::ScopedLockable: N("Attr_ScopedLockable");
  case attr::Section: N("Attr_Section");
  case attr::SelectAny: N("Attr_SelectAny");
  case attr::Sentinel: N("Attr_Sentinel");
  case attr::SetTypestate: N("Attr_SetTypestate");
  case attr::SharedTrylockFunction: N("Attr_SharedTrylockFunction");
  case attr::StdCall: N("Attr_StdCall");
  case attr::SysVABI: N("Attr_SysVABI");
  case attr::TLSModel: N("Attr_TLSModel");
  case attr::TestTypestate: N("Attr_TestTypestate");
  case attr::ThisCall: N("Attr_ThisCall");
  case attr::Thread: N("Attr_Thread");
  case attr::TransparentUnion: N("Attr_TransparentUnion");
  case attr::TryAcquireCapability: N("Attr_TryAcquireCapability");
  case attr::TypeTagForDatatype: N("Attr_TypeTagForDatatype");
  case attr::TypeVisibility: N("Attr_TypeVisibility");
  case attr::Unavailable: N("Attr_Unavailable");
  case attr::Unused: N("Attr_Unused");
  case attr::Used: N("Attr_Used");
  case attr::Uuid: N("Attr_Uuid");
  case attr::VecReturn: N("Attr_VecReturn");
  case attr::VecTypeHint: N("Attr_VecTypeHint");
  case attr::VectorCall: N("Attr_VectorCall");
  case attr::Visibility: N("Attr_Visibility");
  case attr::WarnUnused: N("Attr_WarnUnused");
  case attr::WarnUnusedResult: N("Attr_WarnUnusedResult");
  case attr::Weak: N("Attr_Weak");
  case attr::WeakImport: N("Attr_WeakImport");
  case attr::WeakRef: N("Attr_WeakRef");
  case attr::WorkGroupSizeHint: N("Attr_WorkGroupSizeHint");
  case attr::X86ForceAlignArgPointer: N("Attr_X86ForceAlignArgPointer");
  }
}

bool
DeclarationsVisitor::PreVisitDecl(Decl *D) {
  if (!D) {
    return false;
  }
  if (!optContext.sibling.empty()) {
    const char *name = optContext.sibling.back();
    if (name[0] == '@') {
      optContext.sibling.pop_back();
      return name;
    }
  }
  switch (D->getKind()) {
  case Decl::AccessSpec: ND("Decl_AccessSpec");
  case Decl::Block: ND("Decl_Block");
  case Decl::Captured: ND("Decl_Captured");
  case Decl::ClassScopeFunctionSpecialization: ND("Decl_ClassScopeFunctionSpecialization");
  case Decl::Empty: ND("Decl_Empty");
  case Decl::FileScopeAsm: ND("Decl_FileScopeAsm");
  case Decl::Friend: ND("Decl_Friend");
  case Decl::FriendTemplate: ND("Decl_FriendTemplate");
  case Decl::Import: ND("Decl_Import");
  case Decl::LinkageSpec: ND("Decl_LinkageSpec");
  case Decl::Label: ND("Decl_Label");
  case Decl::Namespace: ND("Decl_Namespace");
  case Decl::NamespaceAlias: ND("Decl_NamespaceAlias");
  case Decl::ObjCCompatibleAlias: ND("Decl_ObjCCompatibleAlias");
  case Decl::ObjCCategory: ND("Decl_ObjCCategory");
  case Decl::ObjCCategoryImpl: ND("Decl_ObjCCategoryImpl");
  case Decl::ObjCImplementation: ND("Decl_ObjCImplementation");
  case Decl::ObjCInterface: ND("Decl_ObjCInterface");
  case Decl::ObjCProtocol: ND("Decl_ObjCProtocol");
  case Decl::ObjCMethod: ND("Decl_ObjCMethod");
  case Decl::ObjCProperty: ND("Decl_ObjCProperty");
  case Decl::ClassTemplate: ND("Decl_ClassTemplate");
  case Decl::FunctionTemplate: ND("Decl_FunctionTemplate");
  case Decl::TypeAliasTemplate: ND("Decl_TypeAliasTemplate");
  case Decl::VarTemplate: ND("Decl_VarTemplate");
  case Decl::TemplateTemplateParm: ND("Decl_TemplateTemplateParm");
  case Decl::Enum: ND("Decl_Enum");
  case Decl::Record: ND("Decl_Record");
  case Decl::CXXRecord: ND("Decl_CXXRecord");
  case Decl::ClassTemplateSpecialization: ND("Decl_ClassTemplateSpecialization");
  case Decl::ClassTemplatePartialSpecialization: ND("Decl_ClassTemplatePartialSpecialization");
  case Decl::TemplateTypeParm: ND("Decl_TemplateTypeParm");
  case Decl::TypeAlias: ND("Decl_TypeAlias");
  case Decl::Typedef: ND("Decl_Typedef");
  case Decl::UnresolvedUsingTypename: ND("Decl_UnresolvedUsingTypename");
  case Decl::Using: ND("Decl_Using");
  case Decl::UsingDirective: ND("Decl_UsingDirective");
  case Decl::UsingShadow: ND("Decl_UsingShadow");
  case Decl::Field: ND("Decl_Field");
  case Decl::ObjCAtDefsField: ND("Decl_ObjCAtDefsField");
  case Decl::ObjCIvar: ND("Decl_ObjCIvar");
  case Decl::Function: ND("Decl_Function");
  case Decl::CXXMethod: ND("Decl_CXXMethod");
  case Decl::CXXConstructor: ND("Decl_CXXConstructor");
  case Decl::CXXConversion: ND("Decl_CXXConversion");
  case Decl::CXXDestructor: ND("Decl_CXXDestructor");
  case Decl::MSProperty: ND("Decl_MSProperty");
  case Decl::NonTypeTemplateParm: ND("Decl_NonTypeTemplateParm");
  case Decl::Var: ND("Decl_Var");
  case Decl::ImplicitParam: ND("Decl_ImplicitParam");
  case Decl::ParmVar: ND("Decl_ParmVar");
  case Decl::VarTemplateSpecialization: ND("Decl_VarTemplateSpecialization");
  case Decl::VarTemplatePartialSpecialization: ND("Decl_VarTemplatePartialSpecialization");
  case Decl::EnumConstant: ND("Decl_EnumConstant");
  case Decl::IndirectField: ND("Decl_IndirectField");
  case Decl::UnresolvedUsingValue: ND("Decl_UnresolvedUsingValue");
  case Decl::OMPThreadPrivate: ND("Decl_OMPThreadPrivate");
  case Decl::ObjCPropertyImpl: ND("Decl_ObjCPropertyImpl");
  case Decl::StaticAssert: ND("Decl_StaticAssert");
  case Decl::TranslationUnit:
    if (OptDisableDeclarations) {
      return false; // stop traverse
    } else {
      return true; // no need to create a child
    }
  }
}

#if 0
bool
DeclarationsVisitor::PreVisitDecl(Decl *D)
{
  switch (D->getKind()) {
  case Decl::Function: {
    const NamedDecl *ND = dyn_cast<const NamedDecl>(D);
    if (ND) {
      raw_string_ostream OS(optContext.tmpstr);
      mangleContext->mangleName(ND, OS);
      xmlNewChild(curNode, nullptr,
                  BAD_CAST ")name", BAD_CAST OS.str().c_str());
    }
    break;
  }
  default:
    break;
  }
  return true;
}
#endif

bool
DeclarationsVisitor::PreVisitNestedNameSpecifier(NestedNameSpecifier *NNS) {
  if (!NNS) {
    N("NestedNameSpecifier_NULL");
  }
  switch (NNS->getKind()) {
  case NestedNameSpecifier::Identifier: N("NestedNameSpecifier_Identifier");
  case NestedNameSpecifier::Namespace: N("NestedNameSpecifier_Namespace");
  case NestedNameSpecifier::NamespaceAlias: N("NestedNameSpecifier_NamespaceAlias");
  case NestedNameSpecifier::Global: N("NestedNameSpecifier_Global");
  case NestedNameSpecifier::Super: N("NestedNameSpecifier_Super");
  case NestedNameSpecifier::TypeSpec: N("NestedNameSpecifier_TypeSpec");
  case NestedNameSpecifier::TypeSpecWithTemplate: N("NestedNameSpecifier_TypeSpecWithTemplate");
  }
}

bool
DeclarationsVisitor::PreVisitNestedNameSpecifierLoc(NestedNameSpecifierLoc NNS) {
  if (!NNS) {
    N("NestedNameSpecifierLoc_NULL");
  }
  switch (NNS.getNestedNameSpecifier()->getKind()) {
  case NestedNameSpecifier::Identifier: N("NestedNameSpecifierLoc_Identifier");
  case NestedNameSpecifier::Namespace: N("NestedNameSpecifierLoc_Namespace");
  case NestedNameSpecifier::NamespaceAlias: N("NestedNameSpecifierLoc_NamespaceAlias");
  case NestedNameSpecifier::Global: N("NestedNameSpecifierLoc_Global");
  case NestedNameSpecifier::Super: N("NestedNameSpecifierLoc_Super");
  case NestedNameSpecifier::TypeSpec: N("NestedNameSpecifierLoc_TypeSpec");
  case NestedNameSpecifier::TypeSpecWithTemplate: N("NestedNameSpecifierLoc_TypeSpecWithTemplate");
  }
}

bool
DeclarationsVisitor::PreVisitDeclarationNameInfo(DeclarationNameInfo NameInfo) {
  DeclarationName DN = NameInfo.getName();
  IdentifierInfo *II = DN.getAsIdentifierInfo();
  const char *Content = II ? II->getNameStart() : nullptr;

  if (!optContext.sibling.empty()) {
    const char *name = optContext.sibling.back();
    if (name[0] == '@') {
      optContext.sibling.pop_back();
      NC(name, Content);
    }
  }

  switch (NameInfo.getName().getNameKind()) {
  case DeclarationName::CXXConstructorName: NC("DeclarationName_CXXConstructorName", Content);
  case DeclarationName::CXXDestructorName: NC("DeclarationName_CXXDestructorName", Content);
  case DeclarationName::CXXConversionFunctionName: NC("DeclarationName_CXXConversionFunctionName", Content);
  case DeclarationName::Identifier: NC("DeclarationName_Identifier", Content);
  case DeclarationName::ObjCZeroArgSelector: NC("DeclarationName_ObjCZeroArgSelector", Content);
  case DeclarationName::ObjCOneArgSelector: NC("DeclarationName_ObjCOneArgSelector", Content);
  case DeclarationName::ObjCMultiArgSelector: NC("DeclarationName_ObjCMultiArgSelector", Content);
  case DeclarationName::CXXOperatorName: NC("DeclarationName_CXXOperatorName", Content);
  case DeclarationName::CXXLiteralOperatorName: NC("DeclarationName_CXXLiteralOperatorName", Content);
  case DeclarationName::CXXUsingDirective: NC("DeclarationName_CXXUsingDirective", Content);
  }
}

///
/// Local Variables:
/// indent-tabs-mode: nil
/// c-basic-offset: 2
/// End:
///