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

#include "internalTracer.h"
#include "forte_architecture_time.h"

#include <algorithm>

std::unordered_map<CStringDictionary::TStringId, std::vector<EventMessage>&> CInternalTracer::smResourceOutputMap;

void CInternalTracer::setResourceOutputMap(std::unordered_map<CStringDictionary::TStringId, std::vector<EventMessage>&>& paResourceOutputMap) {
  smResourceOutputMap = paResourceOutputMap;
}

CInternalTracer::CInternalTracer(CStringDictionary::TStringId instanceName, size_t) : mOutput{smResourceOutputMap.at(instanceName)} {
}

void CInternalTracer::traceInstanceData(std::string paTypeName, std::string paInstanceName, 
        const std::vector<std::string>& paInputs,
        const std::vector<std::string>& paOutputs,
        const std::vector<std::string>& paInternal,
        const std::vector<std::string>& paInternalFB){

  std::vector<std::string> inputs(paInputs.size());
  std::vector<std::string> outputs(paOutputs.size());
  std::vector<std::string> internal(paInternal.size());
  std::vector<std::string> internalFB(paInternalFB.size());

  auto getCStr = [](const auto& str){
    return std::string(str.c_str());
  };

  std::transform(paInputs.cbegin(), paInputs.cend(), inputs.begin(), getCStr);
  std::transform(paOutputs.cbegin(), paOutputs.cend(), outputs.begin(), getCStr);
  std::transform(paInternal.cbegin(), paInternal.cend(), internal.begin(), getCStr);
  std::transform(paInternalFB.cbegin(), paInternalFB.cend(), internalFB.begin(), getCStr);

   mOutput.emplace_back("instanceData", std::make_unique<FBInstanceDataPayload>(paTypeName, paInstanceName, inputs, outputs, internal, internalFB), getNanoSecondsMonotonic());
}

void CInternalTracer::traceReceiveInputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId){
  mOutput.emplace_back("receiveInputEvent", std::make_unique<FBInputEventPayload>(paTypeName, paInstanceName, paEventId), getNanoSecondsMonotonic());
}

void CInternalTracer::traceSendOutputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId, const uint64_t paEventCounter, const std::vector<std::string>& paOutputs){
 mOutput.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>(paTypeName, paInstanceName, paEventId, paEventCounter, paOutputs), getNanoSecondsMonotonic());
}

void CInternalTracer::traceInputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue){
  mOutput.emplace_back("inputData", std::make_unique<FBDataPayload>(paTypeName, paInstanceName, paDataId, paValue), getNanoSecondsMonotonic());
}

void CInternalTracer::traceOutputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue){
  mOutput.emplace_back("outputData", std::make_unique<FBDataPayload>(paTypeName, paInstanceName, paDataId, paValue), getNanoSecondsMonotonic());
}

bool CInternalTracer::isEnabled() {
  return true;
}
