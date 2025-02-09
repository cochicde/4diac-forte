/*******************************************************************************
 * Copyright (c) 2025 Jose Cabral
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Jose Cabral - initial implementation
 *******************************************************************************/

#include "MultiDevice.h"

#include "deviceFactory.h"

#include "core/ecetFake.h"
#include "stdfblib/ita/replay/utils.h"

#include "DebugDevice.h"
#include "OPCUA_DEV.h"
#include "RMT_DEV.h"
#include "stdfblib/ita/replay/ReplayDevice.h"

MultiDevice::MultiDevice(const std::string &paMGRID) : 
    RMT_DEV(paMGRID), mMGRID(paMGRID) {
  
  // avoid creating another MultiDevice in case it was set to it in cmake
  DeviceFactory::setDeviceToCreate(scmDefaultDeviceToCreate);
}

void MultiDevice::awaitShutdown(){
  // wait for the kill signal to arrive
  mKillSignal.get_future().wait();
}

void MultiDevice::setRestartSignal(std::future<void> paSignal){
  mRestartSignalHandler = std::move(paSignal);
}

EMGMResponse MultiDevice::changeExecutionState(EMGMCommandType paCommand) {
  // handle the actual kill signal coming from main
  if(EMGMCommandType::Kill == paCommand){
    killControlledDevice();
    mKillSignal.set_value();
  }
  return EMGMResponse::Ready;
}

int MultiDevice::startDevice() {
  // we don't want to do anything with this device, but rather with mControlledDevice
  RMT_DEV::changeExecutionState(EMGMCommandType::Kill);
  RMT_DEV::awaitShutdown(); // wait for parent shutdown

  // reset all factories to the standard options
  // since each device should set them accordingly
  forte::ita::replay::utils::setFactoriesSettings(forte::ita::replay::utils::FactoriesSettings());

  return resetControlledDevice();
}

void MultiDevice::killControlledDevice(){
  if(mControlledDevice != nullptr){
    mControlledDevice->changeExecutionState(EMGMCommandType::Kill);
    mControlledDevice->awaitShutdown();
  }
}

int MultiDevice::resetControlledDevice(){
  killControlledDevice();

  mControlledDevice = DeviceFactory::createDevice(mMGRID);
  mOpcuaMgr = nullptr; // destroy before creating a new one
  mMultiMgr = nullptr;

  if(auto currentDevice = DeviceFactory::getCurrentDeviceToCreate(); 
      currentDevice == "DebugDevice"){
    mOpcuaMgr = &static_cast<DebugDevice*>(mControlledDevice.get())->mOpcuaMgr;
  }else if (currentDevice == "OPCUA_DEV"){
    mOpcuaMgr = &static_cast<OPCUA_DEV*>(mControlledDevice.get())->mOPCUAMgr;
  } else if (currentDevice == "ReplayDevice"){
    mOpcuaMgr = &static_cast<ReplayDevice*>(mControlledDevice.get())->mOpcuaMgr;
  } else {
    mOpcuaMgr = std::make_unique<OPCUA_MGR>(*mControlledDevice);
  }


  auto result = std::visit([this](auto&& paOpcuaMgr) -> int {

    mMultiMgr = std::make_unique<MultiMGR>(*this, *paOpcuaMgr);
    if(!mMultiMgr->initialize()){
      return -1;
    }

    using T = std::decay_t<decltype(paOpcuaMgr)>;
    if constexpr (std::is_same_v<T, std::unique_ptr<OPCUA_MGR>>) {
      // opcuaMgr belong to us, we need to initialize it
      if (paOpcuaMgr->initialize() != EMGMResponse::Ready) {
        return -1;
      }
    }
    return 0;
  }, mOpcuaMgr);

  if(result != 0){
    return -1;
  }
  mControlledDevice->initialize();
  return mControlledDevice->startDevice();
}
