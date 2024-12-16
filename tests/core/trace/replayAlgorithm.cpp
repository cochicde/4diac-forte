
#include "replayAlgorithm.h"

#include "core/ecetFake.h"
#include "cfb.h"
#include "basicfb.h"

#include "core/ecetFactory.h"
#include "arch/timerHandlerFactory.h"
#include "core/trace/flexibleTracer.h"

#include "utils.h"
#include <thread>

CReplayAlgorithm::CResourceInformation::CResourceInformation(CResource* paResource, 
        const std::vector<EventMessage>& paEvents, 
        std::vector<EventMessage>& paGeneratedTraces) 
        : resource{paResource}, 
          ecet{dynamic_cast<CFakeEventExecutionThread*>(resource->getResourceEventExecution())},
          mEvents{paEvents}, mGeneratedTraces{paGeneratedTraces} {
  ecet->setCallbackForNewEventChain(std::nullopt);
}

const std::set<CStringDictionary::TStringId>& CReplayAlgorithm::getValidTypes(){
  return mValidTypes;
}

CReplayAlgorithm::CReplayAlgorithm(std::function<std::unique_ptr<CDevice>(void)> paCreateDevice) : mCreateDevice{paCreateDevice} {

  EcetFactory::setEcetToCreate(EcetFactory::AvailableEcets::fake);
  TimerHandlerFactory::setTimeHandlerNameToCreate(TimerHandlerFactory::AvailableTimers::fakeTimer);
  CFlexibleTracer::setTracer(CFlexibleTracer::AvailableTracers::Internal);

  auto device = mCreateDevice();

 // Get a list of all types that are not service FB (either Composite or Basic)
  std::function<void(forte::core::CFBContainer*)> iterateContainers;

  iterateContainers = [this, &iterateContainers](forte::core::CFBContainer* paContainer){
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
        mValidTypes.insert(dynamic_cast<CFunctionBlock*>(child)->getFBTypeId());
      }
    }
  };

  iterateContainers(device.get());

  // let it sleep for some time to since if too fast, the stopping signal 
  // comes too early
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  device->changeExecutionState(EMGMCommandType::Kill);
  device->awaitShutdown();
}

CReplayAlgorithm::~CReplayAlgorithm(){
  EcetFactory::setEcetToCreate(EcetFactory::AvailableEcets::standard);
  TimerHandlerFactory::setTimeHandlerNameToCreate(TimerHandlerFactory::AvailableTimers::standard);
  CFlexibleTracer::setTracer(CFlexibleTracer::AvailableTracers::BareCtf);
}

std::unordered_map<std::string, std::vector<EventMessage>> CReplayAlgorithm::execute(const std::unordered_map<std::string, std::vector<EventMessage>>& paExternalEvents){

  auto device = mCreateDevice();

  auto& resourceToMessagesMap = CInternalTracer::getResourceOutputMap();

  std::vector<CResourceInformation> resourceInfomations;

  // get all the information needed for each resource. 
  // Avoid accesing resources not in the parameter map and the device itself (which does not contain an ecet)
  for(auto& [resourceName, messages] : resourceToMessagesMap){
    if(auto resourceStringName = CStringDictionary::getInstance().get(resourceName); 
      paExternalEvents.find(resourceStringName) != paExternalEvents.end() && device->getInstanceNameId() != resourceName){

      resourceInfomations.emplace_back(
          dynamic_cast<CResource*>(forte::unit_test::utils::getFB(device.get(), resourceName)), 
          paExternalEvents.at(resourceStringName), 
          messages
        );
    }
  }

  device->startDevice();

  for(auto& resourceInformation : resourceInfomations){
    reproduceResource(resourceInformation);
    
    resourceInformation.ecet->removeExternalControl();
  }

  // let it sleep for some time to since if too fast, the stopping signal 
  // comes too early
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  device->changeExecutionState(EMGMCommandType::Kill);

  for(auto& resourceInformation : resourceInfomations){
    // resourceInformation.resource->getResourceEventExecution()->resumeSelfSuspend();
    resourceInformation.resource->getResourceEventExecution()->joinEventChainExecutionThread();
  }

  // copy all messages to the result object using std::string as resource name
  std::unordered_map<std::string, std::vector<EventMessage>> expectedMessages;

  for(const auto& [name, messages]: resourceToMessagesMap){
    expectedMessages.insert({CStringDictionary::getInstance().get(name), messages});
  }

  return expectedMessages;
}

void CReplayAlgorithm::reproduceResource(CResourceInformation& paResourceInformation){
  
  // similar implentation as in CFunctionBlock::receiveInputEvent,
  // If the FB type is one that does not interest us (i.e. not a Service Function Block), 
  // we don't do anything and just pass through to the original CFunctionBlock::receiveInputEvent
  // Otherwise, we read the inputs and trace the event, but don't trigger the event itself, meaning
  // that we absorv the event
  auto processOneEvent = [&paResourceInformation, this](TEventEntry paEvent){

    // pass through non interesting events
    if(auto type = CStringDictionary::getInstance().getId(paEvent.mFB->getFBTypeName());
        mValidTypes.find(type) == mValidTypes.end()){
        paEvent.mFB->receiveInputEvent(paEvent.mPortId, paResourceInformation.ecet);
        return;
    }

    if(CFunctionBlock::E_FBStates::Running != paEvent.mFB->getState()){
      return;
    }

    if(paEvent.mPortId >= paEvent.mFB->getFBInterfaceSpec().mNumEIs) {
      return;
    }

    paEvent.mFB->readInputData(paEvent.mPortId);
    paEvent.mFB->traceInputEvent(paEvent.mPortId);
  };

  paResourceInformation.ecet->setCallbackForEventTriggering(processOneEvent);

  // For each of the external events we received as input (with its event counter X), we will advance the ecet 
  // as long as the event counter is less than X, and then trigger the external event X
  for(const auto& externalEvent : paResourceInformation.mEvents){
    
    auto payload = externalEvent.getPayload<FBOutputEventPayload>();

    auto simulateExternalOutputEvent = [&payload, &paResourceInformation] () {
      auto fb = forte::unit_test::utils::getFB(paResourceInformation.resource, CStringDictionary::getInstance().getId(payload->getInstanceName().c_str())); 

      // copy output data to FB 
      for(size_t i = 0; i < payload->mOutputs.size(); i++){
        fb->getDO(i)->fromString(payload->mOutputs[i].c_str());
      }

      // the following will trace and add possible new events to the queue
      fb->sendOutputEvent(payload->mEventId, paResourceInformation.ecet);
    };

    while(paResourceInformation.ecet->getEventCounter() < payload->mEventCounter) {
      paResourceInformation.ecet->triggerNextEvent();
    }

    simulateExternalOutputEvent();
  }

  while(paResourceInformation.ecet->hasEvent()){
    paResourceInformation.ecet->triggerNextEvent();
  }
}
