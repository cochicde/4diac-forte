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

#include <chrono>

CManualEventExecutionThread::CManualEventExecutionThread() :
    CEventChainExecutionThread(){
    mCallback = [this](TEventEntry paEventToAdd){
      this->defaultEventChainCallback(paEventToAdd);
    };
}

void CManualEventExecutionThread::clearExternal() {
  mExternalEventList.clear();
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

void CManualEventExecutionThread::allowInternallyGeneratedEventChains(bool allow, std::function<void(TEventEntry)> callback) {
  mCallback = callback;
  mAllowInternallyGeneratedEventChains = allow;
}

void CManualEventExecutionThread::insertFront(TEventEntry paEvent){
  decltype(mEventList) temp;
  temp.clear();
  temp.push(paEvent);
  while(!mEventList.isEmpty()){
    temp.push(*mEventList.pop());
  }

  while(!temp.isEmpty()){
    mEventList.push(*temp.pop());
  }
  mProcessingEvents = true;
  if(!mProcessingEvents) {
    resumeSelfSuspend();
  }
}

size_t CManualEventExecutionThread::advance(size_t paNumberOfEvents){
  while(paNumberOfEvents != 0){
    {
      std::unique_lock lock(mMutex);
      mCanRun = true;
      mConditionVariable.notify_all();
      while(mProcessingEvents){
        mConditionVariable.wait_for(lock, std::chrono::milliseconds(100), [this](){ return !mCanRun || !mProcessingEvents;});
        if(!mCanRun){
          break;
        }
      }
    }
    if(!mProcessingEvents) {
      break;
    }
    paNumberOfEvents--;
  }
  return paNumberOfEvents;
}


void CManualEventExecutionThread::removeControllFromOutside() {
  mIsControlledFromOutside = false;      
  mCanRun = true;
  mConditionVariable.notify_all();
}

void CManualEventExecutionThread::run(){
  while(isAlive()){
    if(mIsControlledFromOutside)
    {
      std::unique_lock lock(mMutex);
      mConditionVariable.wait(lock, [this](){ return mCanRun; });
    }
    CEventChainExecutionThread::mainRun();
    if(mIsControlledFromOutside)
    {
      mCanRun = false;
      mConditionVariable.notify_all();
    }
  }
}


