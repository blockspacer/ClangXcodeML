#ifndef XCODEMLENVIRONMENT_H
#define XCODEMLENVIRONMENT_H

namespace XcodeMl {

  /*!
   * \brief A mapping from data type identifiers
   * to actual data types.
   */
  class Environment {
  public:
    const TypeRef& operator[](const std::string&) const;
    TypeRef& operator[](const std::string&);
    const TypeRef& at(const std::string&) const;
    TypeRef& at(const std::string&);
    const TypeRef& getReturnType(const std::string&) const;
    void setReturnType(const std::string&, const TypeRef&);
    const std::vector<std::string>& getKeys(void) const;
  private:
    std::map<std::string, TypeRef> map;
    std::map<std::string, TypeRef> returnMap;
    std::vector<std::string> keys;
  };

}

#endif /* XCODEMLENVIRONMENT_H */
