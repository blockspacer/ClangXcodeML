#ifndef XCODEMLVISITORBASE_H
#define XCODEMLVISITORBASE_H

#include "XcodeMlRAV.h"
#include "llvm/ADT/SmallVector.h"
#include "clang/AST/Mangle.h"

#include <libxml/tree.h>
#include <functional>

class TypeTableInfo;

// some members & methods of XcodeMlVisitorBase do not need the info
// of deriving type <Derived>:
// those members & methods are separated from XcodeMlvisitorBase.
// (those methods can be written in the source of XcodeMlVisitorBase.cpp)
class XcodeMlVisitorBaseImpl : public XcodeMlRAVpool {
protected:
    clang::MangleContext *mangleContext;
    xmlNodePtr parentNode;     // the parent node
    xmlNodePtr curNode;        // a candidate of the new chlid.
    TypeTableInfo *typetableinfo;
    std::string contentString; // a temporary holder of xmlNode content
public:
    XcodeMlVisitorBaseImpl() = delete;
    XcodeMlVisitorBaseImpl(const XcodeMlVisitorBaseImpl&) = delete;
    XcodeMlVisitorBaseImpl(XcodeMlVisitorBaseImpl&&) = delete;
    XcodeMlVisitorBaseImpl& operator =(const XcodeMlVisitorBaseImpl&) = delete;
    XcodeMlVisitorBaseImpl& operator =(XcodeMlVisitorBaseImpl&&) = delete;

    explicit XcodeMlVisitorBaseImpl(clang::MangleContext *MC,
                                    xmlNodePtr Parent,
                                    xmlNodePtr CurNode,
                                    TypeTableInfo *TTI);

    xmlNodePtr addChild(const char *Name, const char *Content = nullptr);
    xmlNodePtr addChild(const char *Name, xmlNodePtr N);
    void newChild(const char *Name, const char *Content = nullptr);
    void newProp(const char *Name, int Val, xmlNodePtr N = nullptr);
    void newProp(const char *Name, const char *Val, xmlNodePtr N = nullptr);
    void newComment(const xmlChar *str, xmlNodePtr RN = nullptr);
    void newComment(const char *str, xmlNodePtr RN = nullptr);
    void setLocation(clang::SourceLocation Loc, xmlNodePtr N = nullptr);
    void setContentBySource(clang::SourceLocation LocStart,
                            clang::SourceLocation LocEnd);
};

// Main class: XcodeMlVisitorBase<Derived>
// this is CRTP (Curiously Recurring Template Pattern)
template <class Derived, class OptContext = bool>
class XcodeMlVisitorBase : public XcodeMlVisitorBaseImpl {
protected:
    OptContext optContext;
public:
    XcodeMlVisitorBase() = delete;
    XcodeMlVisitorBase(const XcodeMlVisitorBase&) = delete;
    XcodeMlVisitorBase(XcodeMlVisitorBase&&) = delete;
    XcodeMlVisitorBase& operator =(const XcodeMlVisitorBase&) = delete;
    XcodeMlVisitorBase& operator =(XcodeMlVisitorBase&&) = delete;

    explicit XcodeMlVisitorBase(clang::MangleContext *MC, xmlNodePtr Parent,
                                const char *ChildName,
                                TypeTableInfo *TTI = nullptr)
        : XcodeMlVisitorBaseImpl(MC, Parent,
                                 (ChildName
                                  ? xmlNewTextChild(Parent, nullptr,
                                                    BAD_CAST ChildName,
                                                    nullptr)
                                  : Parent),
                                 TTI),
          optContext() {};
    explicit XcodeMlVisitorBase(XcodeMlVisitorBase *p)
        : XcodeMlVisitorBaseImpl(p->mangleContext, p->curNode, p->curNode,
                                 p->typetableinfo),
          optContext(p->optContext) {};

    Derived &getDerived() { return *static_cast<Derived *>(this); }

#define DISPATCHER(NAME, TYPE)                                          \
    protected:                                                          \
    llvm::SmallVector<std::function<bool (TYPE)>, 3>                    \
    HooksFor##NAME;                                                     \
    public:                                                             \
    bool PreVisit##NAME(TYPE S) {                                       \
        (void)S;                                                        \
        newChild("Traverse" #NAME);                                     \
        return true;                                                    \
    }                                                                   \
    bool Bridge##NAME(TYPE S) override {                                \
        if (!HooksFor##NAME.empty()) {                                  \
            auto Hook = HooksFor##NAME.back();                          \
            HooksFor##NAME.pop_back();                                  \
            newComment("do Hook " #NAME);                               \
            return Hook(S);                                             \
        } else {                                                        \
            return getDerived().Traverse##NAME(S);                      \
        }                                                               \
    }                                                                   \
    bool Traverse##NAME(TYPE S) {                                       \
        Derived V(this);                                                \
        return V.TraverseMe##NAME(S);                                   \
    }                                                                   \
    bool TraverseMe##NAME(TYPE S) {                                     \
        newComment("Traverse" #NAME);                                   \
        if (!getDerived().PreVisit##NAME(S)) {                          \
            return true; /* avoid traverse children */                  \
        }                                                               \
        return getDerived().TraverseChildOf##NAME(S);                   \
    }                                                                   \
    bool TraverseChildOf##NAME(TYPE S) {                                \
        return getDerived().otherside->Bridge##NAME(S);                 \
    }

    DISPATCHER(Stmt, clang::Stmt *);
    DISPATCHER(Type, clang::QualType);
    DISPATCHER(TypeLoc, clang::TypeLoc);
    DISPATCHER(Attr, clang::Attr *);
    DISPATCHER(Decl, clang::Decl *);
    DISPATCHER(NestedNameSpecifier, clang::NestedNameSpecifier *);
    DISPATCHER(NestedNameSpecifierLoc, clang::NestedNameSpecifierLoc);
    DISPATCHER(DeclarationNameInfo, clang::DeclarationNameInfo);
    DISPATCHER(TemplateName, clang::TemplateName);
    DISPATCHER(TemplateArgument, const clang::TemplateArgument &);
    DISPATCHER(TemplateArgumentLoc, const clang::TemplateArgumentLoc &);
    DISPATCHER(ConstructorInitializer, clang::CXXCtorInitializer *);
#undef DISPATCHER
};

#endif /* !XCODEMLVISITORBASE_H */

///
/// Local Variables:
/// mode: c++
/// indent-tabs-mode: nil
/// c-basic-offset: 4
/// End:
///
