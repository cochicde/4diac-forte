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

#include <algorithm>

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
    platform->output.write(reinterpret_cast<const char *>(barectf_packet_buf(&platform->context)),
                           barectf_packet_buf_size(&platform->context));
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
  } else {
    barectf_init(&context, buffer.get(), static_cast<uint32_t>(0), barectfCallbacks, this);
    barectf_enable_tracing(&context, enabled);
  }
}

BarectfPlatformFORTE::BarectfPlatformFORTE(CStringDictionary::TStringId instanceName, size_t bufferSize)
        : BarectfPlatformFORTE(
        traceDirectory / (std::string("trace_") + (CStringDictionary::getInstance().get(instanceName) ?: "null") + "_" + dateCapture() + ".ctf"),
        bufferSize) {
}

BarectfPlatformFORTE::~BarectfPlatformFORTE() {
  if (enabled) {
    if (barectf_packet_is_open(&context) && !barectf_packet_is_empty(&context)) {
      closePacket(this);
    }
    output.flush();
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


void BarectfPlatformFORTE::traceInstanceData(std::string paTypeName, std::string paInstanceName, 
        const std::vector<std::string>& paInputs,
        const std::vector<std::string>& paOutputs,
        const std::vector<std::string>& paInternal,
        const std::vector<std::string>& paInternalFB){

  std::vector<const char *> inputs_c_str(paInputs.size());
  std::vector<const char *> outputs_c_str(paOutputs.size());
  std::vector<const char *> internals_c_str(paInternal.size());
  std::vector<const char *> internalFbs_c_str(paInternalFB.size());

  auto getCstring = [](const auto& str) {
    return str.c_str();
  };

  std::transform(paInputs.cbegin(), paInputs.cend(), inputs_c_str.begin(), getCstring);
  std::transform(paOutputs.cbegin(), paOutputs.cend(), outputs_c_str.begin(), getCstring);
  std::transform(paInternal.cbegin(), paInternal.cend(), internals_c_str.begin(), getCstring);
  std::transform(paInternalFB.cbegin(), paInternalFB.cend(), internalFbs_c_str.begin(), getCstring);

  barectf_default_trace_instanceData(&context,
                                     paTypeName.c_str(),
                                     paInstanceName.c_str(),
                                     static_cast<uint32_t >(inputs_c_str.size()), inputs_c_str.data(),
                                     static_cast<uint32_t >(outputs_c_str.size()), outputs_c_str.data(),
                                     static_cast<uint32_t >(internals_c_str.size()), internals_c_str.data(),
                                     static_cast<uint32_t >(internalFbs_c_str.size()), internalFbs_c_str.data());

}

void BarectfPlatformFORTE::traceReceiveInputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId){
    barectf_default_trace_receiveInputEvent(&context,
                            paTypeName.c_str(),
                            paInstanceName.c_str(),
                            paEventId);
}

void BarectfPlatformFORTE::traceSendOutputEvent(std::string paTypeName, std::string paInstanceName, const uint64_t paEventId){
    barectf_default_trace_sendOutputEvent(&context,
                            paTypeName.c_str(),
                            paInstanceName.c_str(),
                            paEventId);
}

void BarectfPlatformFORTE::traceInputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue){
    barectf_default_trace_inputData(&context,
                            paTypeName.c_str(),
                            paInstanceName.c_str(),
                            paDataId, 
                            paValue.c_str());
}

void BarectfPlatformFORTE::traceOutputData(std::string paTypeName, std::string paInstanceName, uint64_t paDataId, std::string paValue){
    barectf_default_trace_outputData(&context,
                            paTypeName.c_str(),
                            paInstanceName.c_str(),
                            paDataId, 
                            paValue.c_str());
}

void BarectfPlatformFORTE::traceExternalInputEvent(std::string paTypeName, std::string paInstanceName,
    uint64_t paEventId,
    uint64_t paEventCounter, 
    const std::vector<std::string>& paOutputs){
           
    std::vector<const char *> outputs_c_str(paOutputs.size());

    auto getCstring = [](const auto& str) {
      return str.c_str();
    };

   std::transform(paOutputs.cbegin(), paOutputs.cend(), outputs_c_str.begin(), getCstring);

  barectf_default_trace_externalEventInput(
        &context,
        paTypeName.c_str(),
        paInstanceName.c_str(),
        paEventId,
        paEventCounter,
        static_cast<uint32_t >(outputs_c_str.size()), 
        outputs_c_str.data());
}

bool BarectfPlatformFORTE::isEnabled() {
  return barectf_is_tracing_enabled(&context);
}
