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

#include "stdfblib/ita/debug_device/DebugMGR.h"
#include "stdfblib/ita/OPCUA_MGR.h"
#include "stdfblib/ita/replay/deviceReplayer.h"

#include <functional>
#include <string>
#include <vector>
#include <optional>

class CDevice;
class CResource;
class CFakeEventExecutionThread;
class ReplayDevice;

/**
 * @brief Gets a OPCUA_MGR object and adds device and resource methods on top of that
 * Device-level Methods:
 * - Remote Control (bool): void -> Allows enabling/disabling remote control of the device. Pause/Continue buttons in regular debuggers
 * 
 * Resource-level methods:
 * - Trigger Next Event: void -> Execute one event in a resource. Step button in regular debuggers
 * - Add Breakpoint (string): void -> Adds a breakpoint at certain input port of a function block.
 * - Remove Breakpoint (string): void -> Removes a breakpoint at certain input port of a function block.
 */
class ReplayMGR {

public:

  ReplayMGR(ReplayDevice& paDevice, OPCUA_MGR& paOpcuaMgr);
  ~ReplayMGR();
  
  /**
   * @brief Add debugging methods to the OPCUA_MGR object
   * 
   * @return true if no problem occurred while initializing, false otherwise.  
   */
  bool initialize();

private:

  // device on which the methods will be executed
  ReplayDevice& mDevice;
  
  // OpcUa Mgr on top of which the extra debugging methods will be added
  OPCUA_MGR& mOpcuaMgr;

  // Debug Mgr on top of which the extra replaying methods will be added
  DebugMGR mDebugMgr;

  std::unique_ptr<CDeviceReplayer> mDeviceReplayer;

  // the definition of the methods (and therefore its arguments) is done in different functions
  // therefore we need to store the strings for the the arguments so they live until
  // the methods are created in the opc ua server
  std::vector<std::string> mArgumentsInformation;

  /**
   * @brief Store a string and get a reference to it
   * 
   * @param paString String to look for from the stored ones
   * @return reference to the stored string 
   */
  std::string& getArgumentString(std::string paString);

  void addReadTracesMethod();

  void addReplayNextEventMethod();

  static UA_StatusCode onReadTraces(UA_Server*,
    const UA_NodeId*, void*,
    const UA_NodeId*, void*,
    const UA_NodeId*, void*,
    size_t, const UA_Variant* input,
    size_t, UA_Variant*);

  static UA_StatusCode onReplayNextEvent(UA_Server*,
    const UA_NodeId*, void*,
    const UA_NodeId*, void* methodContext,
    const UA_NodeId*, void* objectContext,
    size_t, const UA_Variant*,
    size_t, UA_Variant* output);

};
