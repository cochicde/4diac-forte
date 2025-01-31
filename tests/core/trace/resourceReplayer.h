
#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "EventMessage.h"
#include "core/stringdict.h"

class CFakeEventExecutionThread;
class CFunctionBlock;
class CResource;

namespace forte::core {
  class CFBContainer;
}

/**
 * @brief Algorithm to generate the full series of event in a resource from the subset of output events of Service Function Blocks 
 * 
 */
class  CResourceReplayer {
  public:
  
  /**
   * @brief Constructor
   * 
   * @param paResource the resource being replayed 
   */
  CResourceReplayer(CResource& paResource, std::vector<EventMessage> paExternalEvents);

  ~CResourceReplayer() = default;

  /**
   * @brief Executes the algorithm tha generates the full set of events of the resource
   * 
   * @param paExternalEvents list of output events of Service Function Blocks 
   * @return list of full events of the resource
   */
  std::vector<EventMessage> reproduceAll();

  private:

  CResource& mResource;

  const std::vector<EventMessage> mExternalEvents;

  std::set<CStringDictionary::TStringId> mValidTypes;
};