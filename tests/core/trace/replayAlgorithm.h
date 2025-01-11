
#pragma once

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "EventMessage.h"
#include "core/device.h"

class CFakeEventExecutionThread;
class CFunctionBlock;

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
  CReplayAlgorithm(std::function<std::unique_ptr<CDevice>(void)> paCreateDevice);

  ~CReplayAlgorithm();

  /**
   * @brief Executes the algorithm tha generates the full set of events of the device
   * 
   * @param paExternalEvents list of output events of Service Function Blocks separated by resource name
   * @return list of full events of the device separated by resource name
   */
  std::unordered_map<std::string, std::vector<EventMessage>> execute(const std::unordered_map<std::string, std::vector<EventMessage>>& paExternalEvents);

  /**
   * @brief Get the list of valid function block types that are needed as input to the algorithm (i.e. all Service Function Blocks)
   * 
   * @return list of valid function blocks needed by the algorithm 
   */
  const std::set<CStringDictionary::TStringId>& getValidTypes();

  private:

  std::function<std::unique_ptr<CDevice>(void)> mCreateDevice;

  /**
   * @brief Helper class that contains all needed information of a resource by the algorithm
   * 
   */
  class CResourceInformation {
    public:
    CResourceInformation(CResource* paResource, const std::vector<EventMessage>& paEvents);
    
    CResource* resource;
    CFakeEventExecutionThread* ecet;
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