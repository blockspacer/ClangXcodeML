#ifndef DECLARATIONSVISITOR_H
#define DECLARATIONSVISITOR_H

struct DeclarationsContext {
    explicit DeclarationsContext()
        : nameForDeclRefExpr(nullptr) {};
    explicit DeclarationsContext(DeclarationsContext &DC) 
        : nameForDeclRefExpr(DC.nameForDeclRefExpr) {};
    DeclarationsContext &operator =(const DeclarationsContext &) = delete;
    DeclarationsContext &operator =(DeclarationsContext &&) = delete;

    const char *nameForDeclRefExpr;  // inherited to ancestors
    std::string curFullName; // XXX: unpreferable class member
};

class DeclarationsVisitor
    : public XcodeMlVisitorBase<DeclarationsVisitor, DeclarationsContext> {
public:
    // use base constructors
    using XcodeMlVisitorBase::XcodeMlVisitorBase;

    const char *getVisitorName() const override;
    bool PreVisitStmt(clang::Stmt *);
    bool PreVisitType(clang::QualType);
    bool PreVisitTypeLoc(clang::TypeLoc);
    bool PreVisitAttr(clang::Attr *);
    bool PreVisitDecl(clang::Decl *);
    bool PreVisitNestedNameSpecifier(clang::NestedNameSpecifier *);
    bool PreVisitNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc);
    bool PreVisitDeclarationNameInfo(clang::DeclarationNameInfo);
    bool PreVisitConstructorInitializer(clang::CXXCtorInitializer *CI);

private:
    void setCurFullName(std::string);
    bool WrapExpr(clang::Stmt *);
    bool WrapAsgExpr(void);
    void WrapChild(const char **names);
    void PropChild(const char *name);
    void NameChild(const char *name, clang::Expr *, clang::VarDecl *);
    void WrapCompoundStatementBody(xmlNodePtr, bool);
    void WrapLabelChild(void);
};

#endif /* !DECLARATIONSVISITOR_H */

///
/// Local Variables:
/// mode: c++
/// indent-tabs-mode: nil
/// c-basic-offset: 4
/// End:
///
