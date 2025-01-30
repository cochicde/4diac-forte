
#include "utils.h"
#include "stdfblib/ita/CommandParser.h"

#include <cstring>
#include <set>
#include <functional>

#include "fbcontainer.h"
#include "cfb.h"
#include "basicfb.h"
#include "device.h"

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

std::set<CStringDictionary::TStringId> getValidTypes(CDevice& paDevice){

  std::set<CStringDictionary::TStringId> result;


  // Get a list of all types that are not service FB (either Composite or Basic)
  std::function<void(forte::core::CFBContainer*)> iterateContainers;

  iterateContainers = [&iterateContainers, &result](forte::core::CFBContainer* paContainer){
    for(const auto child : paContainer->getChildren()){
      if(child == nullptr){
        continue;
      }
      if(child->isDynamicContainer()){
        iterateContainers(child);
        continue;
      }
      if(dynamic_cast<CCompositeFB*>(child) == nullptr && 
          dynamic_cast<CBasicFB*>(child) == nullptr && 
          dynamic_cast<CFunctionBlock*>(child) != nullptr){
        result.insert(dynamic_cast<CFunctionBlock*>(child)->getFBTypeId());
      }
    }
  };

  iterateContainers(&paDevice);

  return result;
}

void setFactoriesSettings(FactoriesSettings paFactoriesSettings){
  EcetFactory::setEcetToCreate(paFactoriesSettings.mEcet);
  TimerHandlerFactory::setTimeHandlerNameToCreate(paFactoriesSettings.mTimer);
  CFlexibleTracer::setTracer(paFactoriesSettings.mTracer);
}


} // namespace forte::unit_test::utils