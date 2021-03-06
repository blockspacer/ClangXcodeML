#include <memory>
#include <string>
#include <map>
#include <vector>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include "llvm/ADT/Optional.h"

#include "StringTree.h"
#include "XcodeMlNns.h"
#include "XcodeMlType.h"
#include "XcodeMlTypeTable.h"

#include "SourceInfo.h"

SourceInfo::SourceInfo(xmlXPathContextPtr c,
    const XcodeMl::TypeTable &e,
    const XcodeMl::NnsTable &n,
    Language l)
    : ctxt(c), typeTable(e), nnsTable(n), language(l), uniqueNameIndex(0) {
}

std::string
SourceInfo::getUniqueName() {
  return std::string("__xcodeml_") + std::to_string(++uniqueNameIndex);
}
