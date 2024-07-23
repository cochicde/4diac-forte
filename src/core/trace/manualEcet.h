/*******************************************************************************
 * Copyright (c) 2024 
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
#ifndef _TESTS_CORE_MANUALECET_H_
#define _TESTS_CORE_MANUALECET_H_

#include "../ecet.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <set>
#include <optional>

class CManualEventExecutionThread : public CEventChainExecutionThread{
  public:
    CManualEventExecutionThread();
    ~CManualEventExecutionThread() override = default;


    void startEventChain(TEventEntry paEventToAdd) override;

    void allowInternallyGeneratedEventChains(bool paAllow, std::optional<std::function<void(TEventEntry)>> paCallback = std::nullopt);

    void removeControllFromOutside();

    void triggerOutputEvent(TEventEntry paEvent);

    void insertFront(TEventEntry paEvent);

    void removeFromBack(size_t paNumberOfItemsToRemove);

    TEventEntry* processOneEvent();

    CFunctionBlock* getNextFb();

    static std::set<CStringDictionary::TStringId> smValidTypes;

    uint64_t getEventCounter() const {
      return mEventCounter;
    }

    bool isListEmpty() {
      return mEventList.isEmpty();
    }

private:

    void processEvent(TEventEntry *event);

    void defaultEventChainCallback(TEventEntry paEventEntry);

    void setDefaultEventChainCallback();

    void run() override;
    bool mIsControlledFromOutside{true};
    bool mAllowInternallyGeneratedEventChains{false};
    std::function<void(TEventEntry)> mCallback;

};

#endif /*_TESTS_CORE_MANUALECET_H_*/
