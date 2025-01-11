#pragma once

#include "core/stringdict.h"

#include <string>

class CFunctionBlock;

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

} // namespace forte::unit_test::utils