
#include "replayAlgorithm.h"

#include "core/ecetFake.h"
#include "core/device.h"
#include "utils.h"

#include <thread>
#include <iostream>

CReplayAlgorithm::CResourceInformation::CResourceInformation(CResource& paResource, const std::vector<EventMessage>& paEvents) 
        : mResource{paResource}, 
          mEcet{*dynamic_cast<CFakeEventExecutionThread*>(mResource.getResourceEventExecution())},
          mEvents{paEvents}{
  mEcet.takeExternalControl();
}

CReplayAlgorithm::CReplayAlgorithm(CDevice& paDevice) : mDevice{paDevice} {
}

std::unordered_map<std::string, std::vector<EventMessage>> CReplayAlgorithm::execute(const std::unordered_map<std::string, std::vector<EventMessage>>& paExternalEvents){

  mValidTypes = forte::unit_test::utils::getValidTypes(mDevice);

  std::vector<CResourceInformation> resourceInfomations;

  for(auto child : mDevice.getChildren()){
  auto resource = static_cast<CResource*>(child); // the first generation of children under the device are always resources
   if(std::string resourceStringName = resource->getInstanceName(); 
      paExternalEvents.find(resourceStringName) != paExternalEvents.end()){

      resourceInfomations.emplace_back(
            *resource, 
            paExternalEvents.at(resourceStringName));
    }
  }

  mDevice.startDevice();

  for(auto& resourceInformation : resourceInfomations){
    reproduceResource(resourceInformation);
    resourceInformation.mEcet.removeExternalControl();
  }

  // let it sleep for some time to since if too fast, the stopping signal 
  // comes too early
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  mDevice.changeExecutionState(EMGMCommandType::Kill);

  for(auto& resourceInformation : resourceInfomations){
    resourceInformation.mEcet.joinEventChainExecutionThread();
  }

  // copy all messages to the result object using std::string as resource name
  std::unordered_map<std::string, std::vector<EventMessage>> generatedMessages;

  for(auto resourceInformation : resourceInfomations){

    std::visit(
      [&generatedMessages, &resourceInformation](auto&& paTracer){
        using T = std::decay_t<decltype(paTracer)>;
        if constexpr (std::is_same_v<T, CInternalTracer> == true) {
          generatedMessages.insert({resourceInformation.mResource.getInstanceName(), 
              paTracer.getEvents()});
        }
       }, 
      resourceInformation.mResource.getTracer().getTracerVariant()
    );
  }

  return generatedMessages;
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
        paEvent.mFB->receiveInputEvent(paEvent.mPortId, &paResourceInformation.mEcet);
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

  paResourceInformation.mEcet.setRemoteCallbackForEventTriggering(processOneEvent);

  // For each of the external events we received as input (with its event counter X), we will advance the ecet 
  // as long as the event counter is less than X, and then trigger the external event X
  for(const auto& externalEvent : paResourceInformation.mEvents){
    
    auto payload = externalEvent.getPayload<FBOutputEventPayload>();

    auto simulateExternalOutputEvent = [&payload, &paResourceInformation] () {
      auto fb = forte::unit_test::utils::getFB(&paResourceInformation.mResource, payload->getInstanceName()); 

      if(fb ==nullptr){
        std::cout << "Could not find the FB " << payload->getInstanceName() << " -> aborting..." << std::endl;
        std::abort();
      }

      // copy output data to FB 
      for(size_t i = 0; i < payload->mOutputs.size(); i++){
        fb->getDO(i)->fromString(payload->mOutputs[i].c_str());
      }

      // the following will trace and add possible new events to the queue
      fb->sendOutputEvent(payload->mEventId, &paResourceInformation.mEcet);
    };

    while(paResourceInformation.mEcet.getEventCounter() < payload->mEventCounter) {
      paResourceInformation.mEcet.triggerNextEvent();
    }

    simulateExternalOutputEvent();
  }

  while(paResourceInformation.mEcet.hasEvent()){
    paResourceInformation.mEcet.triggerNextEvent();
  }
}
