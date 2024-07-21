

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

#ifndef FLEXIBLE_TRACER_H
#define FLEXIBLE_TRACER_H

#include <memory>
#include <string>

#include "tracer.h"
#include "stringdict.h"

class CFlexibelTracer final : public CTracer {
public: 

    CFlexibelTracer(CStringDictionary::TStringId instanceName, size_t bufferSize);

    ~CFlexibelTracer() override = default;

    CFlexibelTracer(const CFlexibelTracer&) = delete;
    CFlexibelTracer& operator=(const CFlexibelTracer&) = delete;


    void traceInstanceData(std::string paTypeName, std::string paInstanceName, 
        const std::vector<std::string>& paInputs,
        const std::vector<std::string>& paOutputs,
        const std::vector<std::string>& paInternal,
        const std::vector<std::string>& paInternalFB) override;

    void traceReceiveInputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId) override;

    void traceSendOutputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId, const uint64_t paEventCounter, const std::vector<std::string>& paOutputs) override;

    void traceInputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) override;

    void traceOutputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue) override;

    bool isEnabled() override;

    static void setTracer(std::string paTracerName);

private:

  static constexpr const char* scmBareCTFName = "barectf";

  static std::string mCurrentTracer;

  std::unique_ptr<CTracer> mTracer;
};

#endif // FLEXIBLE_TRACER_H
