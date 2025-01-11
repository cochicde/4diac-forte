
#include "utils.h"
#include "stdfblib/ita/CommandParser.h"

#include <cstring>

#include "fbcontainer.h"


namespace forte::unit_test::utils {

CFunctionBlock* getFB(forte::core::CFBContainer* paContainer, const std::string& paFunctionBlockName) {
  if(paContainer == nullptr){
    return nullptr;
  }

  forte::core::TNameIdentifier id;

  // copy from OPCUA_MGR with some modifications
  std::string fbNamePart(paFunctionBlockName);
  size_t index = fbNamePart.find_first_of(".");
  while (index != std::string::npos) {
    id.pushBack(CStringDictionary::getInstance().insert(fbNamePart.substr(0, index).c_str()));
    fbNamePart = fbNamePart.substr(index + 1);
    index = fbNamePart.find_first_of(".");
  }
  id.pushBack(CStringDictionary::getInstance().insert(fbNamePart.substr(0, index).c_str()));

  forte::core::TNameIdentifier::CIterator nonConstIterator(id.begin());
  return paContainer->getFB(nonConstIterator);
}


CFunctionBlock* getFB(forte::core::CFBContainer* paContainer, CStringDictionary::TStringId paFunctionBlockName) {
  if(paContainer == nullptr){
    return nullptr;
  }
  forte::core::TNameIdentifier id;
  id.pushBack(paFunctionBlockName);
  forte::core::TNameIdentifier::CIterator nonConstIterator(id.begin());
  return paContainer->getFB(nonConstIterator);
}

} // namespace forte::unit_test::utils