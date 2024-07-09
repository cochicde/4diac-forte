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

#include "manualComLayer.h"

namespace forte::com_infra {

CManualComLayer::CManualComLayer(CComLayer* paUpperLayer, CBaseCommFB* paComFB) : CComLayer(paUpperLayer, paComFB)
{
}


EComResponse CManualComLayer::openConnection(char *) {
  // TODO: register somehere with 
// the mFb and this 
  return EComResponse::e_InitOk;
} 

void CManualComLayer::closeConnection() {
  return;
  }  

EComResponse CManualComLayer::sendData(void *, unsigned int ) {
  /* do nothing for now. Compare with outputs in the future*/
  return EComResponse::e_InitOk; 
};

EComResponse CManualComLayer::recvData(const void *, unsigned int ) {
  return EComResponse::e_InitOk;
}


EComResponse CManualComLayer::processInterrupt() {
  return EComResponse::e_InitOk;
};

} // forte::com_infra
