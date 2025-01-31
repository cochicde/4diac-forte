
#include "resourceReplayer.h"

#include "core/resource.h"
#include "utils.h"
#include "core/ecetFake.h"

#include <iostream>

CResourceReplayer::CResourceReplayer(CResource& paResource, std::vector<EventMessage> paExternalEvents) 
  : mResource{paResource}, mExternalEvents{std::move(paExternalEvents)} {
  mValidTypes = forte::unit_test::utils::getValidTypes(mResource);
}

std::vector<EventMessage> CResourceReplayer::reproduceAll(){
  
  auto& ecet = *dynamic_cast<CFakeEventExecutionThread*>(mResource.getResourceEventExecution());

  // similar implentation as in CFunctionBlock::receiveInputEvent,
  // If the FB type is one that does not interest us (i.e. not a Service Function Block), 
  // we don't do anything and just pass through to the original CFunctionBlock::receiveInputEvent
  // Otherwise, we read the inputs and trace the event, but don't trigger the event itself, meaning
  // that we absorv the event
  auto processOneEvent = [&ecet, this](TEventEntry paEvent){

    // pass through non interesting events
    if(auto type = CStringDictionary::getInstance().getId(paEvent.mFB->getFBTypeName());
        mValidTypes.find(type) == mValidTypes.end()){
        paEvent.mFB->receiveInputEvent(paEvent.mPortId, &ecet);
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

  ecet.setRemoteCallbackForEventTriggering(processOneEvent);

  // For each of the external events we received as input (with its event counter X), we will advance the ecet 
  // as long as the event counter is less than X, and then trigger the external event X
  for(const auto& externalEvent : mExternalEvents){
    
    auto payload = externalEvent.getPayload<FBOutputEventPayload>();

    auto simulateExternalOutputEvent = [&payload, &ecet, this] () {
      auto fb = forte::unit_test::utils::getFB(&mResource, payload->getInstanceName()); 

      if(fb ==nullptr){
        std::cout << "Could not find the FB " << payload->getInstanceName() << " -> aborting..." << std::endl;
        std::abort();
      }

      // copy output data to FB 
      for(size_t i = 0; i < payload->mOutputs.size(); i++){
        fb->getDO(i)->fromString(payload->mOutputs[i].c_str());
      }

      // the following will trace and add possible new events to the queue
      fb->sendOutputEvent(payload->mEventId, &ecet);
    };

    while(ecet.getEventCounter() < payload->mEventCounter) {
      ecet.triggerNextEvent();
    }

    simulateExternalOutputEvent();
  }

  while(ecet.hasEvent()){
    ecet.triggerNextEvent();
  }

  return std::visit(
    [this](auto&& paTracer) -> std::vector<EventMessage> {
      using T = std::decay_t<decltype(paTracer)>;
      if constexpr (std::is_same_v<T, CInternalTracer> == true) {
        return paTracer.getEvents();
      }
      return {};
    }, 
    mResource.getTracer().getTracerVariant()
  );
}
