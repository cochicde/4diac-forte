/*******************************************************************************
 * Copyright (c) 2022 Primetals Technologies Austria GmbH
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Christoph Binder - initial implementation
 *******************************************************************************/
#include "faketimerha.h"
#include "../../core/devexec.h"
#include "ecet.h"
#include "device.h"

CTimerHandler* CTimerHandler::createTimerHandler(CDeviceExecution &pa_poDeviceExecution) {
  return new CFakeTimerHandler(pa_poDeviceExecution);
}

CFakeTimerHandler::CFakeTimerHandler(CDeviceExecution &pa_poDeviceExecution) :
  CTimerHandler(pa_poDeviceExecution), sleepTime(0), fakeSleepFb(nullptr) {
}

CFakeTimerHandler::~CFakeTimerHandler() {
  disableHandler();
}

void CFakeTimerHandler::run() {
  CEventChainExecutionThread *execThread = 0;
  while(isAlive()) {
    if(sleepTime > 0) {
      execThread = getExecThread(); //get execThread for every sleep fake, since resource potentially changed
      while(sleepTime > 0) {
        nextTick();
        trackFakeForteTime();
        sleepTime--;
        while(execThread != 0 && execThread->isProcessingEvents()) {
          sleepThread(0);
        }
      }
      if(fakeSleepFb) {
        fakeSleepFb->receiveInputEvent(-1, nullptr);
      }
    } else {
      sleepThread(1000);
    }
  }
}

CEventChainExecutionThread* CFakeTimerHandler::getExecThread() {
  const char *resNname = 0;
  for(forte::core::CFBContainer::TFunctionBlockList::Iterator itRunner = mDeviceExecution.getDevice().getFBList().begin();
      itRunner != mDeviceExecution.getDevice().getFBList().end(); ++itRunner) {
    resNname = static_cast<CResource*>(*itRunner)->getInstanceName();
    if(strcmp(resNname, "ROBOT_TEST_RES") == 0) {
      return static_cast<CResource*>(*itRunner)->getResourceEventExecution();
    }
  }
  return 0;
}

void CFakeTimerHandler::setSleepTime(CIEC_TIME &t, CFunctionBlock *fb) {
  fakeSleepFb = fb;
  if(sleepTime == 0) {
    sleepTime = t.getInMilliSeconds();
    if(sleepTime == 0) {
      if(fakeSleepFb) {
        fakeSleepFb->receiveInputEvent(-1, nullptr); // no computation time necessary in case of zero sleeptime
      }
    }
  }
}

void CFakeTimerHandler::enableHandler(void) {
  start();
}

void CFakeTimerHandler::disableHandler(void) {
  end();
}

void CFakeTimerHandler::setPriority(int) {
  // empty implementation
}

int CFakeTimerHandler::getPriority(void) const {
  return 1;
}
