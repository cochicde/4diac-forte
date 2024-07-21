/*******************************************************************************
 * Copyright (c) 2018 - fortiss GmbH
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Jose Cabral - initial implementation and rework communication infrastructure
 *******************************************************************************/

#include "manualEcet.h"
#include "funcbloc.h"

#include <chrono>

std::set<CStringDictionary::TStringId> CManualEventExecutionThread::smValidTypes;


CManualEventExecutionThread::CManualEventExecutionThread() :
    CEventChainExecutionThread(){
  setDefaultEventChainCallback();
}

void CManualEventExecutionThread::setDefaultEventChainCallback(){
  mCallback = [this](TEventEntry paEventToAdd){
    this->defaultEventChainCallback(paEventToAdd);
  };
}

void CManualEventExecutionThread::startEventChain(TEventEntry paEventToAdd) {
  if(!mAllowInternallyGeneratedEventChains){
    return;
  }
  mCallback(paEventToAdd);
}

void CManualEventExecutionThread::defaultEventChainCallback(TEventEntry paEventEntry){
  CEventChainExecutionThread::startEventChain(paEventEntry);
}

void CManualEventExecutionThread::allowInternallyGeneratedEventChains(bool allow, std::optional<std::function<void(TEventEntry)>> callback) {
  mAllowInternallyGeneratedEventChains = allow;
  if(allow){
    mCallback = callback.value();
  } else {
    setDefaultEventChainCallback();
  }
}

void CManualEventExecutionThread::insertFront(TEventEntry paEvent){
  decltype(mEventList) temp;
  temp.push(paEvent);
  while(!mEventList.isEmpty()){
    temp.push(*mEventList.pop());
  }

  while(!temp.isEmpty()){
    mEventList.push(*temp.pop());
  }
}

void CManualEventExecutionThread::triggerOutputEvent(TEventEntry paEvent){
  paEvent.mFB->sendOutputEvent(paEvent.mPortId, this);
}


size_t CManualEventExecutionThread::advance(size_t paNumberOfEvents){
  while(paNumberOfEvents != 0){
    {
      std::unique_lock lock(mMutex);
      mCanRun = true;
      mConditionVariable.notify_all();
      mConditionVariable.wait(lock, [this](){ return !mCanRun;});
    }
    paNumberOfEvents--;
  }
  return paNumberOfEvents;
}
#include <iostream>
void CManualEventExecutionThread::advanceUntil(size_t paEventPosition){
  while(mEventCounter != paEventPosition){
    if(mEventCounter >= paEventPosition) {
      std::cout << "Event queue advanced more than expected" << std::endl; 
    }
    std::unique_lock lock(mMutex);
    mCanRun = true;
    mConditionVariable.notify_all();
    mConditionVariable.wait(lock, [this](){ return !mCanRun;});
  }

  // special case for events that don't have any output connection
  // they are still traced but the next event will have the same
  // event counter number. As a temp solution, we let these events
  // trigger since the event counter will remain the same
  // while(mEventCounter - 1 == paEventPosition){
    if(mEventList.isEmpty()){
      return;
    }

    auto nextEvent = *mEventList.pop(); // get a copy, but need to pop for it
    insertFront(nextEvent);
    if(auto type = CStringDictionary::getInstance().getId(nextEvent.mFB->getFBTypeName());
      smValidTypes.find(type) == smValidTypes.end()){
        return;
    }
    // we can trigger the next event since it will be absorved anyway
    processEvent(&nextEvent);
  // }
}

void CManualEventExecutionThread::triggerEventOnCounter(TEventEntry paEvent, size_t paEventCounter, const std::vector<std::string>& paOutputs){
  auto insertEventToList = [&paEvent, &oaOutputs] (){
    for(size_t i = 0; i < paOutputs.size(); i++){
      paEvent.mFB->getDO(i)->fromString(paOutputs[i].c_str());
    }

    triggerOutputEvent(paEvent);
  };
  
  if(paEventCounter == 0){
    insertEventToList();
    return;
  }
  
  while(mEventCounter <= paEventCounter){
    // store amount of traces
    // test process event,
    // if more than paEvent coutner, rollback
    // else, keep going

  }

  if(mEventCounter >= paEventCounter) {
    std::cout << "Event queue advanced more than expected" << std::endl;
    std::abort();
  }


  // special case for events that don't have any output connection
  // they are still traced but the next event will have the same
  // event counter number. As a temp solution, we let these events
  // trigger since the event counter will remain the same
  if(mEventCounter - 1 == paEventCounter && !mEventList.isEmpty()){
    auto nextEvent = *mEventList.pop(); // get a copy, but need to pop for it
    if(nextEvent.mFB->getInstanceName() == paEvent.mFB->getInstanceName()){
        // we can trigger the next event since it will be absorved anyway
        processEvent(&nextEvent);
    } else {
      // insert the event back to the list
      insertFront(nextEvent);
    }
  }

  for(size_t i = 0; i < paOutputs.size(); i++){
    paEvent.mFB->getDO(i)->fromString(paOutputs[i].c_str());
  }

  triggerOutputEvent(paEvent);

}

void CManualEventExecutionThread::removeControllFromOutside() {
  mIsControlledFromOutside = false;      
  mCanRun = true;
  mConditionVariable.notify_all();
}

// we don't need to complexities of separate threads
void CManualEventExecutionThread::run(){
  selfSuspend();
}
//   while(isAlive()){
//     if(mIsControlledFromOutside)
//     {
//       std::unique_lock lock(mMutex);
//       mConditionVariable.wait(lock, [this](){ return mCanRun; });
//     }
//     CManualEventExecutionThread::mainRun();
//     if(mIsControlledFromOutside)
//     {
//       mCanRun = false;
//       mConditionVariable.notify_all();
//     }
//   }
// }

void CManualEventExecutionThread::mainRun(){

  // insert external event inputs in the front
  // if(!mExternalEventList.isEmpty()){
  //   std::vector<TEventEntry> temp;
  //   while(!mEventList.isEmpty()){
  //     temp.push_back(*mEventList.pop());
  //   }
  //   while(!mExternalEventList.isEmpty()){
  //     mEventList.push(*mExternalEventList.pop());
  //   }
  //   for(auto& event : temp){
  //     mEventList.push(event);
  //   }
  // }


  processEvent(mEventList.pop());
 
}

void CManualEventExecutionThread::processEvent(TEventEntry *event){
 if(nullptr == event){
    // mProcessingEvents = false;
    // selfSuspend();
    // mProcessingEvents = true; //set this flag here to true as well in case the suspend just went through and processing was not finished
    return;
  }

  // // absorv external events but count up to keep as in the original run
  // if(event->mPortId == cgExternalEventID){
  //   return;
  // }

  if(auto type = CStringDictionary::getInstance().getId(event->mFB->getFBTypeName());
    smValidTypes.find(type) == smValidTypes.end()){
    event->mFB->receiveInputEvent(event->mPortId, this);
  }
  else {
    // Absorv event: copy the inputs and trace but don't execute inside the FB
    if(CFunctionBlock::E_FBStates::Running == event->mFB->getState()){
      if(event->mPortId < event->mFB->mInterfaceSpec->mNumEIs) {
        event->mFB->readInputData(event->mPortId);
        #ifdef FORTE_SUPPORT_MONITORING
          // Count Event for monitoring
          event->mFB->mEIMonitorCount[event->mPortId]++;
        #endif //FORTE_SUPPORT_MONITORING
          event->mFB->traceInputEvent(event->mPortId);
      }
      // don't execute event of the function block
    }
  }
}