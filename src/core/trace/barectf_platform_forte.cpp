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

#include "barectf_platform_forte.h"

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <vector>

#include "forte_architecture_time.h"
#include <forte_sem.h>

// #include "atomic_queue.h"

class TraceCPP {
  public:
    uint64_t mEventId;
    uint64_t mEventCounter;
    std::vector<uint64_t> mInstanceNames;
    std::vector<uint8_t> mOutputs;
};
class AsyncWorker {
  public:
    AsyncWorker(BarectfPlatformFORTE &paTracer) : mTracer{paTracer} {
      mFuture = std::async(std::launch::async, &AsyncWorker::process, this);
      resetBuffer(mBuffer);
      resetBuffer(mIncommingPackets);
      resetBuffer(mToProcess);
      mThreadStartSignal.get_future().wait();
    }

    void resetBuffer(std::vector<TraceCPP> &paBuffer) {
      paBuffer = std::vector<TraceCPP>(bufferSize);
      for (size_t i = 0; i < bufferSize; i++) {
        paBuffer[i].mInstanceNames.reserve(20);
        paBuffer[i].mOutputs.reserve(1000);
      }
    }

    uint64_t getClock() {
      return mCurrentClock;
    }

    ~AsyncWorker() {
      mAlive = false;
      mNewDataAvailable = true;
      {
        std::lock_guard guard(mMutex);
        std::swap(mIncommingPackets, mBuffer);
        mAddedTracesBuffer = mAddedTraces;
        mNewDataAvailable = true;
        mNewDataSignal.notify_one();
      }
      mFuture.wait();
    }

    void addTrace(const std::vector<uint64_t> &paInstanceNames,
                  const uint64_t paEventId,
                  const uint64_t paEventCounter,
                  const std::vector<uint8_t> &paOutputs) {
      mBuffer[mAddedTraces++] = TraceCPP(paEventId, paEventCounter, paInstanceNames, paOutputs);
      if (mAddedTraces == bufferSize) {

        std::lock_guard guard(mMutex);
        std::swap(mIncommingPackets, mBuffer);
        mAddedTracesBuffer = mAddedTraces;
        mNewDataAvailable = true;
        mNewDataSignal.notify_one();
        mAddedTraces = 0;
      }
    }
    void process() {
      mThreadStartSignal.set_value();
      while (mAlive) {
        {
          std::unique_lock lock(mMutex);
          mNewDataSignal.wait(lock, [this] { return mNewDataAvailable; });

          std::swap(mToProcess, mIncommingPackets);
          mNewDataAvailable = false;
        }

        for (size_t i = 0; i < mAddedTracesBuffer; i++) {
          mCurrentClock = mToProcess[i].mEventCounter;
          mTracer.traceSendOutputEvent(static_cast<uint32_t>(mToProcess[i].mInstanceNames.size()),
                                       mToProcess[i].mInstanceNames.data(), mToProcess[i].mEventId,
                                       static_cast<uint32_t>(mToProcess[i].mOutputs.size()),
                                       mToProcess[i].mOutputs.data());
        }
      }
    }

  private:
    static constexpr size_t bufferSize = 100000;
    size_t mAddedTraces{0};
    size_t mAddedTracesBuffer{0};

    bool mAlive{true};
    bool mNewDataAvailable{false};
    uint64_t mCurrentClock{0};

    std::mutex mMutex;
    std::future<void> mFuture;
    std::condition_variable mNewDataSignal;
    forte::arch::CSemaphore mSuspendSemaphore;
    std::vector<TraceCPP> mIncommingPackets;
    std::vector<TraceCPP> mToProcess;
    std::vector<TraceCPP> mBuffer;
    BarectfPlatformFORTE &mTracer;
    std::promise<void> mThreadStartSignal;
};

// class AsyncWorker {
//   public:
//     AsyncWorker(BarectfPlatformFORTE &paTracer) : mTracer{paTracer} {
//       mFuture = std::async(std::launch::async, &AsyncWorker::process, this);
//       mThreadStartSignal.get_future().wait();
//     }
//     ~AsyncWorker() {
//       mAlive = false;
//       addTrace({}, 0, 0, {}); // add dummy
//       mFuture.wait();
//     }
//     uint64_t getClock() {
//       return mCurrentClock;
//     }

//     void addTrace(const std::vector<uint64_t> &paInstanceNames,
//                   const uint64_t paEventId,
//                   const uint64_t paEventCounter,
//                   const std::vector<uint8_t> &paOutputs) {
//       mQueue.push(TraceCPP(paEventId, paEventCounter, paInstanceNames, paOutputs));
//     }
//     void process() {
//       mThreadStartSignal.set_value();
//       while (mAlive) {
//         auto toTrace = mQueue.pop();
//         mCurrentClock = toTrace.mEventCounter;
//         mTracer.traceSendOutputEvent(static_cast<uint32_t>(toTrace.mInstanceNames.size()),
//                                      toTrace.mInstanceNames.data(), toTrace.mEventId,
//                                      static_cast<uint32_t>(toTrace.mOutputs.size()), toTrace.mOutputs.data());
//       }
//     }

//   private:
//     static constexpr size_t bufferSize = 1000;
//     uint64_t mCurrentClock{0};
//     bool mAlive{true};
//     std::future<void> mFuture;
//     BarectfPlatformFORTE &mTracer;
//     std::promise<void> mThreadStartSignal;
//     atomic_queue::AtomicQueue2<TraceCPP, bufferSize, false, false, false, true> mQueue;
// };

std::filesystem::path BarectfPlatformFORTE::traceDirectory = std::filesystem::path();
bool BarectfPlatformFORTE::enabled = false;

void barectfSetup(std::string directory) {
  BarectfPlatformFORTE::setup(directory);
}

void BarectfPlatformFORTE::setup(std::string directory) {
  traceDirectory = std::filesystem::path(directory).make_preferred();
  if (traceDirectory.empty()) {
    DEVLOG_INFO("[TRACE_CTF]: no output directory given, disabling TRACE_CTF\n");
    enabled = false;
    return;
  }
  if (traceDirectory.is_relative()) {
    traceDirectory = std::filesystem::absolute(traceDirectory);
  }

  if (std::filesystem::is_directory(traceDirectory)) {
    DEVLOG_INFO("[TRACE_CTF]: enabling TRACE_CTF, output in \"%s\"\n", traceDirectory.string().c_str());
    enabled = true;
  } else {
    DEVLOG_INFO("[TRACE_CTF]: non-existent output directory given \"%s\", disabling TRACE_CTF\n",
                traceDirectory.string().c_str());
    enabled = false;
  }
}

uint64_t BarectfPlatformFORTE::getClock(void *const data) {
  BarectfPlatformFORTE *platform = static_cast<BarectfPlatformFORTE *>(data);
  return platform->mWorker->getClock();
}

int BarectfPlatformFORTE::isBackendFull(void *data) {
  BarectfPlatformFORTE *platform = static_cast<BarectfPlatformFORTE *>(data);
  return platform->output.fail();
}

void BarectfPlatformFORTE::openPacket(void *data) {
  if (enabled) {
    BarectfPlatformFORTE *platform = static_cast<BarectfPlatformFORTE *>(data);
    barectf_default_open_packet(&platform->context);
  }
}

void BarectfPlatformFORTE::closePacket(void *data) {
  if (enabled) {
    BarectfPlatformFORTE *platform = static_cast<BarectfPlatformFORTE *>(data);
    barectf_default_close_packet(&platform->context);
    platform->output.write(reinterpret_cast<const char *>(barectf_packet_buf(&platform->context)),
                           barectf_packet_buf_size(&platform->context));
  }
}

const struct barectf_platform_callbacks BarectfPlatformFORTE::barectfCallbacks = {.default_clock_get_value = getClock,
                                                                                  .is_backend_full = isBackendFull,
                                                                                  .open_packet = openPacket,
                                                                                  .close_packet = closePacket};

BarectfPlatformFORTE::BarectfPlatformFORTE(std::filesystem::path filename, size_t bufferSize) :
    buffer(enabled ? new uint8_t[bufferSize] : nullptr),
    mWorker(std::make_unique<AsyncWorker>(*this)) {
  if (enabled) {
    output = std::ofstream(filename, std::ios::binary);
    barectf_init(&context, buffer.get(), static_cast<uint32_t>(bufferSize), barectfCallbacks, this);
    barectf_enable_tracing(&context, enabled);
    openPacket(this);
  } else {
    barectf_init(&context, buffer.get(), static_cast<uint32_t>(0), barectfCallbacks, this);
    barectf_enable_tracing(&context, enabled);
  }
}

BarectfPlatformFORTE::BarectfPlatformFORTE(CStringDictionary::TStringId instanceName, size_t bufferSize) :
    BarectfPlatformFORTE(traceDirectory / (std::string("trace_") + (CStringDictionary::get(instanceName) ?: "null") +
                                           "_" + dateCapture() + ".ctf"),
                         bufferSize) {
}

BarectfPlatformFORTE::~BarectfPlatformFORTE() {
  if (enabled) {
    if (barectf_packet_is_open(&context)) {
      closePacket(this);
    }
    output.flush();
  }
}

std::string BarectfPlatformFORTE::dateCapture() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  const auto millisecondsPart =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
  struct tm ptm;
  forte_localtime(&time, &ptm);
  std::ostringstream stream;
  stream << std::put_time(&ptm, "%Y%m%d_%H%M%S");
  stream << std::setfill('0') << std::setw(3) << millisecondsPart;
  return stream.str();
}

void BarectfPlatformFORTE::traceSendOutputEvent2(const std::vector<uint64_t> &paInstanceNames,
                                                 const uint64_t paEventId,
                                                 uint64_t paEventCounter,
                                                 const std::vector<uint8_t> &paOutputs) {
  mWorker->addTrace(paInstanceNames, paEventId, paEventCounter, paOutputs);
}
