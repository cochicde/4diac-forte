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

#include <mutex>
#include <condition_variable>

class CManualEventExecutionThread : public CEventChainExecutionThread{
  public:
    CManualEventExecutionThread();
    ~CManualEventExecutionThread() override = default;

    /**
     * @brief Advance the execution a certain amount of events. 
     * 
     * @param paNumberOfEvents Number of events to advance
     * 
     * @return number of events that could not be advanced because the the execution didn't have more events
     * and got into a suspended state. If all events where advanced, 0 is returned
    */
    size_t advance(size_t paNumberOfEvents);

    void removeControllFromOutside();

private:
    void run() override;
    std::mutex mMutex;
    std::condition_variable mConditionVariable;
    bool mCanRun{false};
    bool mIsControlledFromOutside{true};
};

#endif /*_TESTS_CORE_MANUALECET_H_*/
