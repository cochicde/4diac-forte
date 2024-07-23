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
#include <iostream>

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

void CManualEventExecutionThread::removeControllFromOutside() {
  mIsControlledFromOutside = false;      
  resumeSelfSuspend();
}

// we don't need to complexities of separate threads
void CManualEventExecutionThread::run(){
  while(isAlive()){
    if(mIsControlledFromOutside){
      selfSuspend();
    } 
    CEventChainExecutionThread::mainRun();
  }
}
CFunctionBlock* CManualEventExecutionThread::getNextFb(){
  auto nextEvent = mEventList.pop(); // get a copy, but need to pop for it
  if(nextEvent == nullptr){
    return nullptr;
  }

  insertFront(*nextEvent);
  return nextEvent->mFB;

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

void CManualEventExecutionThread::removeFromBack(size_t paNumberOfItemsToRemove){
  std::vector<TEventEntry> temp;
  while(!mEventList.isEmpty()){
    temp.push_back(*mEventList.pop());
  }

  while(paNumberOfItemsToRemove-- != 0){
    temp.pop_back();
    mEventCounter--;
  }

  for(auto& event : temp){
    mEventList.push(event);
  }
}

TEventEntry* CManualEventExecutionThread::processOneEvent(){

  auto event = mEventList.pop();
  if(nullptr == event){
    return event;
  }

  if(auto type = CStringDictionary::getInstance().getId(event->mFB->getFBTypeName());
    smValidTypes.find(type) == smValidTypes.end()){
    event->mFB->receiveInputEvent(event->mPortId, this);
    return event;
  }
  
  // Absorv event: copy the inputs and trace but don't execute inside the FB

  if(CFunctionBlock::E_FBStates::Running != event->mFB->getState()){
    return event;
  }
  
  if(event->mPortId >= event->mFB->mInterfaceSpec->mNumEIs) {
    return event;
  }

  event->mFB->readInputData(event->mPortId);
  event->mFB->traceInputEvent(event->mPortId);

  return event;
}

void CManualEventExecutionThread::triggerOutputEvent(TEventEntry paEvent){
  paEvent.mFB->sendOutputEvent(paEvent.mPortId, this);
}

// void CManualEventExecutionThread::triggerEventOnCounter(TEventEntry paEvent, size_t paEventCounter){
  
//   while(mEventCounter <= paEventCounter){
//     // store amount of traces
//     // test process event,
//     // if more than paEvent coutner, rollback
//     // else, keep going

//   }

//   if(mEventCounter >= paEventCounter) {
//     std::cout << "Event queue advanced more than expected" << std::endl;
//     std::abort();
//   }


//   // special case for events that don't have any output connection
//   // they are still traced but the next event will have the same
//   // event counter number. As a temp solution, we let these events
//   // trigger since the event counter will remain the same
//   if(mEventCounter - 1 == paEventCounter && !mEventList.isEmpty()){
//     auto nextEvent = *mEventList.pop(); // get a copy, but need to pop for it
//     if(nextEvent.mFB->getInstanceName() == paEvent.mFB->getInstanceName()){
//         // we can trigger the next event since it will be absorved anyway
//         processEvent(&nextEvent);
//     } else {
//       // insert the event back to the list
//       insertFront(nextEvent);
//     }
//   }
