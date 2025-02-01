
#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

#include "EventMessage.h"
#include "core/stringdict.h"
#include "core/event.h"

class CResource;
class CFakeEventExecutionThread;

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

  /**
   * @brief Reproduce the next event
   * 
   * @return the event that was executed, std::nullopt if nothing was executed 
   */
  std::optional<TEventEntry> reproduceNextEvent();

  private:

  CResource& mResource;

  CFakeEventExecutionThread& mEcet;

  size_t mStepperIndex{0};

  const std::vector<EventMessage> mExternalEvents;
};