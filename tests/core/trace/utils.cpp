
#include "utils.h"

#include "fbcontainer.h"

namespace forte::unit_test::utils {

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