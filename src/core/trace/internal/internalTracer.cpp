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

CInternalTracer::CInternalTracer(CStringDictionary::TStringId, size_t) {
}

void CInternalTracer::traceSendOutputEvent(const char *const paTypeName,
                                           const char *const paInstanceName,
                                           const uint64_t paEventId
#ifdef FORTE_TRACE_CTF_REPLAY_DEBUGGING
                                           ,
                                           const uint64_t paEventCounter,
                                           const uint32_t paOutputsLength,
                                           const char *const *const paOutputs
#endif
) {
#ifdef FORTE_TRACE_CTF_REPLAY_DEBUGGING

  std::vector<std::string> outputs(paOutputsLength);
  fillStringsVector(paOutputs, paOutputsLength, outputs);

  mEvents.emplace_back(
      "sendOutputEvent",
      std::make_unique<FBOutputEventPayload>(paTypeName, paInstanceName, paEventId, paEventCounter, outputs),
      getNanoSecondsMonotonic());
#else // FORTE_TRACE_CTF_REPLAY_DEBUGGING
  mEvents.emplace_back("sendOutputEvent", std::make_unique<FBOutputEventPayload>(paTypeName, paInstanceName, paEventId),
                       getNanoSecondsMonotonic());
#endif // FORTE_TRACE_CTF_REPLAY_DEBUGGING
}

bool CInternalTracer::isEnabled() {
  return true;
}

void CInternalTracer::fillStringsVector(const char *const *const paIn,
                                        const uint32_t paLen,
                                        std::vector<std::string> &paOut) {
  for (uint32_t i = 0; i < paLen; i++) {
    paOut[i] = paIn[i];
  }
}

const std::vector<EventMessage> &CInternalTracer::getEvents() const {
  return mEvents;
}
