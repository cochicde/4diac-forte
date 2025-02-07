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

#pragma once

#include "RMT_DEV.h"
#include "ReplayMGR.h"

/**
 * @brief Device that adds replay commands to the device. The commands are defined in ReplayMGR
 * 
 */
class ReplayDevice : public RMT_DEV {
public:

  ReplayDevice(const std::string &paMGRID = "localhost:61499");
  ~ReplayDevice() override = default;

  int startDevice() override;

  EMGMResponse executeMGMCommand(forte::core::SManagementCMD &paCommand) override;

  void startControlling();

  OPCUA_MGR mOpcuaMgr;

  ReplayMGR mReplayMgr;

  private:

  bool mAlreadyControlled{false};
};
