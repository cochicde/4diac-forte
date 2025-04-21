/*******************************************************************************
 * Copyright (c) 2022 Martin Erich Jobst
 *               2023 Primetals Technologies Austria GmbH
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Martin Jobst
 *      - initial implementation
 *******************************************************************************/
#ifndef BARECTF_PLATFORM_FORTE_H
#define BARECTF_PLATFORM_FORTE_H

#include <string>
#include <fstream>
#include <memory>
#include <filesystem>

#include "stringdict.h"

#include "barectf.h"

/**
 * @brief BareCTF tracer
 *
 * It receives the data and it traces into the corresponding files
 */
class BarectfPlatformFORTE final {
  public:
    BarectfPlatformFORTE(std::filesystem::path filename, size_t bufferSize);
    BarectfPlatformFORTE(CStringDictionary::TStringId instanceName, size_t bufferSize);
    ~BarectfPlatformFORTE();

    BarectfPlatformFORTE(const BarectfPlatformFORTE &) = delete;
    BarectfPlatformFORTE &operator=(const BarectfPlatformFORTE &) = delete;

    BarectfPlatformFORTE(BarectfPlatformFORTE &&) = delete;
    BarectfPlatformFORTE &operator=(BarectfPlatformFORTE &&) = delete;

    void traceSendOutputEvent(const uint32_t paInstanceNameLength,
                              const uint64_t *paInstanceNames,
                              const uint64_t paEventId,
                              const uint64_t paEventCounter,
                              const uint32_t paOutputsLength,
                              const uint8_t *paOutputs) {
      mCurrentClock = paEventCounter;
      barectf_default_trace_sendOutputEvent(&context, paInstanceNameLength, paInstanceNames, paEventId, paOutputsLength,
                                            paOutputs);
    }

    bool isEnabled() {
      return barectf_is_tracing_enabled(&context);
    }

    static void setup(std::string directory);

  private:
    std::ofstream output;
    std::unique_ptr<uint8_t[]> buffer;
    barectf_default_ctx context;
    uint64_t mCurrentClock{0};

    static bool enabled;
    static std::filesystem::path traceDirectory;

    static uint64_t getClock(void *const data);
    static int isBackendFull(void *data);
    static void openPacket(void *data);
    static void closePacket(void *data);
    static const struct barectf_platform_callbacks barectfCallbacks;
    static std::string dateCapture();
};

#endif // BARECTF_PLATFORM_FORTE_H
