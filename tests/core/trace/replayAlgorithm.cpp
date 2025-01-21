
#include "replayAlgorithm.h"

#include "core/ecetFake.h"
#include "cfb.h"
#include "basicfb.h"

#include "core/ecetFactory.h"
#include "arch/timerHandlerFactory.h"
#include "core/trace/flexibleTracer.h"

#include "utils.h"
#include <thread>
#include <iostream>

CReplayAlgorithm::CResourceInformation::CResourceInformation(CResource* paResource, const std::vector<EventMessage>& paEvents) 
        : resource{paResource}, 
          ecet{dynamic_cast<CFakeEventExecutionThread*>(resource->getResourceEventExecution())},
          mEvents{paEvents}{
   if(ecet != nullptr){
    ecet->takeExternalControl();
  }
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

  std::function<void(forte::core::CFBContainer*)> iterateContainers;

  std::vector<CResource*> resources;

  iterateContainers = [this, &iterateContainers, &resources](forte::core::CFBContainer* paContainer){
    if(paContainer == nullptr){
        return;
    }
    if(auto resource = dynamic_cast<CResource*>(paContainer); resource != nullptr){ 
      resources.push_back(resource);
    }

    for(const auto child : paContainer->getChildren()){
      iterateContainers(child);
    }
  };

  iterateContainers(device.get());

  std::vector<CResourceInformation> resourceInfomations;

  for(auto resource : resources){
   if(std::string resourceStringName = resource->getInstanceName(); 
      paExternalEvents.find(resourceStringName) != paExternalEvents.end()){

      resourceInfomations.emplace_back(
            resource, 
            paExternalEvents.at(resourceStringName));
    }
  }

  device->startDevice();

  for(auto& resourceInformation : resourceInfomations){
    reproduceResource(resourceInformation);
    
    if(resourceInformation.ecet != nullptr){
      resourceInformation.ecet->removeExternalControl();
    }
  }

  // let it sleep for some time to since if too fast, the stopping signal 
  // comes too early
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  device->changeExecutionState(EMGMCommandType::Kill);

  for(auto& resourceInformation : resourceInfomations){
    if(resourceInformation.ecet != nullptr){
      resourceInformation.ecet->joinEventChainExecutionThread();
    }
  }

  // copy all messages to the result object using std::string as resource name
  std::unordered_map<std::string, std::vector<EventMessage>> generatedMessages;

  for(auto resourceInformation : resourceInfomations){

    std::visit(
      [&generatedMessages, &resourceInformation](auto&& paTracer){
        using T = std::decay_t<decltype(paTracer)>;
        if constexpr (std::is_same_v<T, CInternalTracer> == true) {
          generatedMessages.insert({resourceInformation.resource->getInstanceName(), 
              paTracer.getEvents()});
        }
       }, 
      resourceInformation.resource->getTracer().getTracerVariant()
    );
  }

  return generatedMessages;
}

void CReplayAlgorithm::reproduceResource(CResourceInformation& paResourceInformation){
  
  if(paResourceInformation.ecet == nullptr){
    return;
  }

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

  paResourceInformation.ecet->setRemoteCallbackForEventTriggering(processOneEvent);

  // For each of the external events we received as input (with its event counter X), we will advance the ecet 
  // as long as the event counter is less than X, and then trigger the external event X
  for(const auto& externalEvent : paResourceInformation.mEvents){
    
    auto payload = externalEvent.getPayload<FBOutputEventPayload>();

    auto simulateExternalOutputEvent = [&payload, &paResourceInformation] () {
      auto fb = forte::unit_test::utils::getFB(paResourceInformation.resource, payload->getInstanceName()); 

      if(fb ==nullptr){
        std::cout << "Could not find the FB " << payload->getInstanceName() << " -> aborting..." << std::endl;
        std::abort();
      }

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
