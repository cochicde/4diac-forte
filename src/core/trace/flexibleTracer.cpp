

/*******************************************************************************
 * Copyright (c) 2024 Jose Cabral
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Jose Cabral
 *      - initial implementation
 *******************************************************************************/

#include "flexibleTracer.h"

std::string CFlexibelTracer::mCurrentTracer;

CFlexibelTracer::CFlexibelTracer(CStringDictionary::TStringId instanceName, size_t bufferSize) {
  if(mCurrentTracer == ""){
    mTracer = std::make_unique<BarectfPlatformFORTE>(instanceName, bufferSize);
  } else {
    mTracer = std::make_unique<CInternalTracer>(instanceName, bufferSize);
  }
}

void CFlexibelTracer::traceInstanceData(std::string paTypeName, std::string paInstanceName, 
    const std::vector<std::string>& paInputs,
    const std::vector<std::string>& paOutputs,
    const std::vector<std::string>& paInternal,
    const std::vector<std::string>& paInternalFB) {
      mTracer->traceInstanceData(paTypeName, paInstanceName, paInputs, paOutputs, paInternal, paInternalFB);
}

void CFlexibelTracer::traceReceiveInputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId) {
  mTracer->traceReceiveInputEvent(paTypeName, paInstanceName, paEventId);
}

void CFlexibelTracer::traceSendOutputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId) {
  mTracer->traceSendOutputEvent(paTypeName, paInstanceName, paEventId);
}

void CFlexibelTracer::traceInputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) {
  mTracer->traceInputData(paTypeName, paInstanceName, paDataId, paValue);
}

void CFlexibelTracer::traceOutputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) {
  mTracer->traceOutputData(paTypeName, paInstanceName, paDataId, paValue);
}

void CFlexibelTracer::traceExternalInputEvent(
  std::string paTypeName, 
  std::string paInstanceName,
  uint64_t paEventId,
  uint64_t paEventCounter, 
  const std::vector<std::string>& paOutputs) {
    mTracer->traceExternalInputEvent(paTypeName, paInstanceName, paEventId, paEventCounter, paOutputs);
  }

bool CFlexibelTracer::isEnabled() {
  return mTracer->isEnabled();
}

void CFlexibelTracer::setTracer(std::string paTracerName) {
  CFlexibelTracer::mCurrentTracer = std::move(paTracerName);
}

