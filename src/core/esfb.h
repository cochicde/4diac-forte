/*******************************************************************************
 * Copyright (c) 2005 - 2014 Profactor GmbH, ACIN, fortiss GmbH
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Thomas Strasser, Alois Zoitl, Gerhard Ebenhofer
 *      - initial implementation and rework communication infrastructure
 *******************************************************************************/
#ifndef _ESFB_H_
#define _ESFB_H_

#include "funcbloc.h"

#ifdef FORTE_TRACE_CTF
#include "trace/barectf_platform_forte.h"
#include "resource.h"
#include "ecet.h"
#endif

/*!\ingroup CORE\brief Base-Class for all event sources.
 */
class CEventSourceFB : public CFunctionBlock{
private:

/* \brief the event chain executor used by this ES.
 */
  CEventChainExecutionThread *mEventChainExecutor;

protected:
  TEventEntry mEventSourceEventEntry; //! the event entry to start the event chain

public:
  CEventSourceFB(forte::core::CFBContainer &paContainer, const SFBInterfaceSpec *paInterfaceSpec,
                 CStringDictionary::TStringId paInstanceNameId) :
          CFunctionBlock(paContainer, paInterfaceSpec, paInstanceNameId),
          mEventChainExecutor(nullptr),
          mEventSourceEventEntry(this, cgExternalEventID) {
  }

  ~CEventSourceFB() override = default;
  void setEventChainExecutor(CEventChainExecutionThread *paEventChainExecutor) { mEventChainExecutor = paEventChainExecutor; };
  CEventChainExecutionThread * getEventChainExecutor() { return mEventChainExecutor; };

  TEventEntry *getEventSourceEventEntry() { return &mEventSourceEventEntry; };

#ifdef FORTE_TRACE_CTF
 void traceExternalEventInput(TEventID paEIID) override {
    if(paEIID != cgExternalEventID){
      return;
    }
    
    if(auto& tracer = getResource()->getTracer(); tracer.isEnabled()){
      std::vector<std::string> outputs(mInterfaceSpec->mNumDOs);

      for(TPortId i = 0; i < outputs.size(); ++i) {
        CIEC_ANY *value = getDO(i);
        std::string &valueString = outputs[i];
        valueString.reserve(value->getToStringBufferSize());
        value->toString(valueString.data(), valueString.capacity());
      }

      tracer.traceExternalInputEvent(
            getFBTypeName() ?: "null",
            getInstanceName() ?: "null",
            static_cast<uint64_t>(paEIID),
            mEventChainExecutor->mEventCounter,
            outputs);
  }
 }
#endif // FORTE_TRACE_CTF

};

#define EVENT_SOURCE_FUNCTION_BLOCK_CTOR(fbclass) \
 fbclass(const CStringDictionary::TStringId paInstanceNameId, forte::core::CFBContainer &paContainer) : \
 CEventSourceFB(paContainer, &scmFBInterfaceSpec, paInstanceNameId)

#endif /*_ESFB_H_*/
