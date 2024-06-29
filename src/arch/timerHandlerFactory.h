/*******************************************************************************
 * Copyright (c) 2024 
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Jose Cabral - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "timerha.h"

#include <string>

class CTimerHandlerFactory {
  public:

    /*!\brief create the timer handler and set the parameter pointer with the the new timer handler.
      *
      * This function is not implemented in the standardtimerhandler and has to be implemented in the specific implementation.
      * implementations should check that not two timerhanlders can be created.
      */
    static CTimerHandler* createTimerHandler(CDeviceExecution &paDeviceExecution);

    static void setTimeHandlerNameToCreate(std::string paName);
  
  private:

    static std::string smTimeHandlerToCreate;

};