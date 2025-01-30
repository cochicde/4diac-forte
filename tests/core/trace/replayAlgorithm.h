
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
class CDevice;
class CResource;

namespace forte::core {
  class CFBContainer;
}

/**
 * @brief Algorithm to generate the full series of event in a device from the subset of output events of Service Function Blocks 
 * 
 */
class CReplayAlgorithm {
  public:
  
  /**
   * @brief Constructor
   * 
   * @param paCreateDevice a function that creates the device for which the generation of events must be done 
   */
  CReplayAlgorithm(CDevice& paDevice);

  ~CReplayAlgorithm() = default;

  /**
   * @brief Executes the algorithm tha generates the full set of events of the device
   * 
   * @param paExternalEvents list of output events of Service Function Blocks separated by resource name
   * @return list of full events of the device separated by resource name
   */
  std::unordered_map<std::string, std::vector<EventMessage>> execute(const std::unordered_map<std::string, std::vector<EventMessage>>& paExternalEvents);

  private:

  CDevice& mDevice;

  /**
   * @brief Helper class that contains all needed information of a resource by the algorithm
   * 
   */
  class CResourceInformation {
    public:
    CResourceInformation(CResource& paResource, const std::vector<EventMessage>& paEvents);
    
    CResource& mResource;
    CFakeEventExecutionThread& mEcet;
    const std::vector<EventMessage>& mEvents;
  };

  /**
   * @brief Generate the resources of one specific resource
   * 
   * @param paHelper information about the resource
   */
  void reproduceResource(CResourceInformation& paHelper);


  std::set<CStringDictionary::TStringId> mValidTypes;

};