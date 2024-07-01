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

#ifndef INTERNAL_TRACER_H
#define INTERNAL_TRACER_H

#include <unordered_map>
#include <vector>

#include "tracer.h"

#include "EventMessage.h"
#include "stringdict.h"

class CInternalTracer final : public CTracer {
public: 

    CInternalTracer(CStringDictionary::TStringId instanceName, size_t bufferSize);

    virtual ~CInternalTracer() = default;

    CInternalTracer(const CInternalTracer&) = delete;
    CInternalTracer& operator=(const CInternalTracer&) = delete;


    void traceInstanceData(std::string paTypeName, std::string paInstanceName, 
        const std::vector<std::string>& paInputs,
        const std::vector<std::string>& paOutputs,
        const std::vector<std::string>& paInternal,
        const std::vector<std::string>& paInternalFB) override;

    void traceReceiveInputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId) override;

    void traceSendOutputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId) override;

    void traceInputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) override;

    void traceOutputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) override;

    void traceExternalInputEvent(std::string paTypeName, std::string paInstanceName,
        uint64_t paEventId,
        uint64_t paEventCounter, 
        const std::vector<std::string>& paOutputs = {}) override;

    bool isEnabled() override;

    static void setResourceOutputMap(std::unordered_map<CStringDictionary::TStringId, std::vector<EventMessage>&>& paResourceOutputMap);

private: 
  std::vector<EventMessage>& mOutput;

  static std::unordered_map<CStringDictionary::TStringId, std::vector<EventMessage>&> smResourceOutputMap;


};

#endif // INTERNAL_TRACER_H
