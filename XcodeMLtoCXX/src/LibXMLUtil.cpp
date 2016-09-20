#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include "LibXMLUtil.h"
#include "XMLString.h"

static xmlXPathObjectPtr getNodeSet(
    xmlNodePtr, const char*, xmlXPathContextPtr);

void XPathObjectReleaser::operator() (xmlXPathObjectPtr ptr) {
  xmlXPathFreeObject(ptr);
}

/*!
 * \brief Search for an element that matches given XPath expression.
 * \pre \c node is not null.
 * \pre \c xpathExpr is not null.
 * \pre \c xpathCtxt is not null.
 * \return nullptr if any element in \c node doesn't match \c xpathExpr
 * \return The first element that matches \c xpathExpr.
 */
xmlNodePtr findFirst(xmlNodePtr node, const char* xpathExpr, xmlXPathContextPtr xpathCtxt) {
  xmlXPathObjectPtr xpathObj = getNodeSet(node, xpathExpr, xpathCtxt);
  if (!xpathObj) {
    return nullptr;
  }
  xmlNodePtr val = xpathObj->nodesetval->nodeTab[0];
  xmlXPathFreeObject(xpathObj);
  return val;
}

size_t length(xmlXPathObjectPtr obj) {
  return (obj->nodesetval)? obj->nodesetval->nodeNr:0;
}

xmlNodePtr nth(xmlXPathObjectPtr obj, size_t n) {
  return obj->nodesetval->nodeTab[n];
}

bool isTrueProp(xmlNodePtr node, const char* name, bool default_value) {
  if (!xmlHasProp(node, BAD_CAST name)) {
    return default_value;
  }
  std::string value = static_cast<XMLString>(xmlGetProp(node, BAD_CAST name));
  if (value == "1" || value == "true") {
    return true;
  } else if (value == "0" || value == "false") {
    return false;
  }
  throw std::runtime_error("Invalid attribute value");
}

std::vector<xmlNodePtr> findNodes(
    xmlNodePtr node,
    const char* xpathExpr,
    xmlXPathContextPtr xpathCtxt
) {
  xmlXPathObjectPtr xpathObj = getNodeSet(node, xpathExpr, xpathCtxt);
  if (!xpathObj) {
    return {};
  }
  std::vector<xmlNodePtr> nodes;
  xmlNodeSetPtr matchedNodes = xpathObj->nodesetval;
  const int len = matchedNodes->nodeNr;
  for (int i = 0; i < len; ++i) {
    nodes.push_back(matchedNodes->nodeTab[i]);
  }
  xmlXPathFreeObject(xpathObj);
  return nodes;
}

std::string getNameFromIdNode(
    xmlNodePtr idNode,
    xmlXPathContextPtr ctxt
) {
  xmlNodePtr nameNode = findFirst(idNode, "name", ctxt);
  return static_cast<XMLString>(xmlNodeGetContent(nameNode));
}

bool isNaturalNumber(const std::string& prop) {
  return std::all_of(prop.begin(), prop.end(), isdigit);
}

static xmlXPathObjectPtr getNodeSet(
    xmlNodePtr node,
    const char* xpathExpr,
    xmlXPathContextPtr xpathCtxt
) {
  assert(node && xpathExpr);
  xmlXPathSetContextNode(node, xpathCtxt);
  xmlXPathObjectPtr xpathObj = xmlXPathNodeEval(
      node,
      BAD_CAST xpathExpr,
      xpathCtxt);
  if (!xpathObj) {
    return nullptr;
  }
  xmlNodeSetPtr matchedNodes = xpathObj->nodesetval;
  if (!matchedNodes || !matchedNodes->nodeNr) {
    xmlXPathFreeObject(xpathObj);
    return nullptr;
  }
  return xpathObj;
}

