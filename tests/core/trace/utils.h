#pragma once

#include "core/stringdict.h"

#include "core/ecetFactory.h"
#include "arch/timerHandlerFactory.h"
#include "core/trace/flexibleTracer.h"

#include <string>
#include <set>

class CFunctionBlock;
class CDevice;

namespace forte::core {
  class CFBContainer;
}

namespace forte::unit_test::utils {

  /**
   * @brief  get the function block instance out of the container. This is just a shorthand for creating the needed parameters for the CFBContainer::getFB
   * 
   * @param paContainer Device where the to look for the resource 
   * @param paFunctionBlockName name of the function block
   * @return a pointer to the resource with the provided name, nullptr if a resource with the provided name does not exist
   */
CFunctionBlock* getFB(forte::core::CFBContainer* paContainer, const std::string& paFunctionBlockName);

  /**
   * @brief  Same as the previous function, but using other type of input parameter
   * 
   * @param paContainer Device where the to look for the resource 
   * @param paFunctionBlockName name of the function block
   * @return a pointer to the resource with the provided name, nullptr if a resource with the provided name does not exist
   */
CFunctionBlock* getFB(forte::core::CFBContainer* paContainer, CStringDictionary::TStringId paFunctionBlockName);


  /**
   * @brief Get the list of valid function block types that are needed as input to the algorithm (i.e. all Service Function Blocks)
   * 
   * @return list of valid function blocks needed by the algorithm 
   */
  std::set<CStringDictionary::TStringId> getValidTypes(forte::core::CFBContainer& paDevice);

  struct FactoriesSettings {
    EcetFactory::AvailableEcets mEcet{EcetFactory::AvailableEcets::standard};
    TimerHandlerFactory::AvailableTimers mTimer{TimerHandlerFactory::AvailableTimers::standard};
    CFlexibleTracer::AvailableTracers mTracer{CFlexibleTracer::AvailableTracers::BareCtf};
  };

  void setFactoriesSettings(FactoriesSettings paFactoriesSettings);

} // namespace forte::unit_test::utils