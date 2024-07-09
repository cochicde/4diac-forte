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

#include "manualHandler.h"

DEFINE_HANDLER(CManualHandler)

CManualHandler::CManualHandler(CDeviceExecution& paDeviceExecution) : CExternalEventHandler(paDeviceExecution){
  
}
CManualHandler::~CManualHandler(){

}

void CManualHandler::enableHandler(){
 // do nothing  
}

void CManualHandler::disableHandler(){
   // do nothing  
}

void CManualHandler::setPriority(int){
   // do nothing  
}

int CManualHandler::getPriority() const{
  return 0;
}