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

#include <iomanip>
#include <chrono>

#include "forte_architecture_time.h"

std::filesystem::path BarectfPlatformFORTE::traceDirectory = std::filesystem::path();
bool BarectfPlatformFORTE::enabled = false;

void barectfSetup(std::string directory) {
  BarectfPlatformFORTE::setup(directory);
}

void BarectfPlatformFORTE::setup(std::string directory) {
  traceDirectory = std::filesystem::path(directory).make_preferred();
  if(traceDirectory.empty()) {
      DEVLOG_INFO("[TRACE_CTF]: no output directory given, disabling TRACE_CTF\n");
      enabled = false;
      return;
  }
  if (traceDirectory.is_relative()) {
      traceDirectory = std::filesystem::absolute(traceDirectory);
  }

  if(std::filesystem::is_directory(traceDirectory)) {
      DEVLOG_INFO("[TRACE_CTF]: enabling TRACE_CTF, output in \"%s\"\n", traceDirectory.string().c_str());
      enabled = true;
  } else {
      DEVLOG_INFO("[TRACE_CTF]: non-existent output directory given \"%s\", disabling TRACE_CTF\n", traceDirectory.string().c_str());
      enabled = false;
  }
}

uint64_t BarectfPlatformFORTE::getClock(void *const) {
  return getNanoSecondsMonotonic();
}

int BarectfPlatformFORTE::isBackendFull(void *data) {
  BarectfPlatformFORTE *platform = static_cast<BarectfPlatformFORTE *>(data);
  return platform->output.fail();
}

void BarectfPlatformFORTE::openPacket(void *data) {
  if(enabled) {
    BarectfPlatformFORTE *platform = static_cast<BarectfPlatformFORTE *>(data);
    barectf_default_open_packet(&platform->context);
  }
}

void BarectfPlatformFORTE::closePacket(void *data) {
  if(enabled) {
    BarectfPlatformFORTE *platform = static_cast<BarectfPlatformFORTE *>(data);
    barectf_default_close_packet(&platform->context);
    // platform->output.write(reinterpret_cast<const char *>(barectf_packet_buf(&platform->context)),
    //                        barectf_packet_buf_size(&platform->context));
  }
}

const struct barectf_platform_callbacks BarectfPlatformFORTE::barectfCallbacks = {
        .default_clock_get_value = getClock,
        .is_backend_full = isBackendFull,
        .open_packet = openPacket,
        .close_packet = closePacket
};

BarectfPlatformFORTE::BarectfPlatformFORTE(std::filesystem::path filename, size_t bufferSize)
        : buffer(enabled ? new uint8_t[bufferSize] : nullptr) {
  if(enabled) {
    output = std::ofstream(filename, std::ios::binary);
    barectf_init(&context, buffer.get(), static_cast<uint32_t>(bufferSize), barectfCallbacks, this);
    barectf_enable_tracing(&context, enabled);
    openPacket(this);
    // mWorker = std::thread([this]{
    //   this->work();
    // });
  } else {
    barectf_init(&context, buffer.get(), static_cast<uint32_t>(0), barectfCallbacks, this);
    barectf_enable_tracing(&context, enabled);
  }
}

void BarectfPlatformFORTE::traceSendOutputEvent(const char * const paTypeName, const char * const paInstanceName, const uint64_t paEventId, const uint64_t paEventCounter, const uint32_t paOutputsLength, const char * const * const paOutputs) {
    barectf_default_trace_sendOutputEvent(&context,
                              paTypeName,
                              paInstanceName,
                              paEventId,
                              paEventCounter,
                              paOutputsLength, 
                              paOutputs);
    return;


    mToTraceQ.emplace(paTypeName,
                  paInstanceName,
                  paEventId,
                  paEventCounter,
                  paOutputsLength,
                  paOutputs);
    
    if(mToTraceQ.size() == 1000) {
      std::unique_lock lock(mQeueMutex);
      mShouldWrite = true;
      mCond_var.notify_all();
      mCond_var.wait(lock, [this]{return !mShouldWrite;});
    }
}

BarectfPlatformFORTE::BarectfPlatformFORTE(CStringDictionary::TStringId instanceName, size_t bufferSize)
        : BarectfPlatformFORTE(
        traceDirectory / (std::string("trace_") + (CStringDictionary::get(instanceName) ?: "null") + "_" + dateCapture() + ".ctf"),
        bufferSize) {
}

BarectfPlatformFORTE::~BarectfPlatformFORTE() {
  if (enabled) {
    if (barectf_packet_is_open(&context)) {
      closePacket(this);
    }
    output.flush();
  }
  mShouldLive = false;
  mCond_var.notify_all();
  if(mWorker.joinable()){
    mWorker.join();
  }
}

 void BarectfPlatformFORTE::work(){
      std::queue<ToTrace> temp;

      while(mShouldLive){
        {
          std::unique_lock lock(mQeueMutex);
          mCond_var.wait(lock, [this]{return mShouldWrite || !mShouldLive;});
          temp = std::move(mToTraceQ);
          mToTraceQ = std::queue<ToTrace>();
          mShouldWrite = false;
          mCond_var.notify_all();
          // while(mToTraceQ.size() != 0){
          //   mToTraceQ.pop();
          // }
        }
        while(temp.size() != 0){
          auto& toTrace = temp.front();
          auto outputSize = toTrace.mOutputs.size();
          std::vector<const char *> outputs_c_str(outputSize);

          for(std::size_t i = 0; i < toTrace.mOutputs.size(); ++i) {
            outputs_c_str[i] = toTrace.mOutputs[i].c_str();
          }
          // barectf_default_trace_sendOutputEvent(&context,
          //                     toTrace.mTypeName.c_str(),
          //                     toTrace.mInstanceName.c_str(),
          //                     toTrace.mEventId,
          //                     toTrace.mEventCounter,
          //                     static_cast<uint32_t >(toTrace.mOutputs.size()), 
          //                     outputs_c_str.data());
          temp.pop();
        }

      }

    }

std::string BarectfPlatformFORTE::dateCapture() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  const auto millisecondsPart = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
  struct tm ptm;
  forte_localtime(&time, &ptm);
  std::ostringstream stream;
  stream << std::put_time(&ptm, "%Y%m%d_%H%M%S");
  stream << std::setfill('0') << std::setw(3) << millisecondsPart;
  return stream.str();
}
