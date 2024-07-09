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
#ifndef _MANUAL_COMLAYER_H_
#define _MANUAL_COMLAYER_H_

#include "comlayer.h"

namespace forte::com_infra {

  class CBaseCommFB;

  class CManualComLayer final : public CComLayer {
    public:
      CManualComLayer(CComLayer* paUpperLayer, CBaseCommFB* paComFB);

      ~CManualComLayer() override = default;

      /*!\brief Configure the current layer and perform necessary means to setup the connection
        *
        * Depending on the layers functionality different things have to be performed here.
        * This can range from doing nothing to establishing a TCP session.
        *
        * @param paLayerParameter configuration data for this layer
        * @return status of the opening process
        *      - e_InitOk of the opening process was successful
        */
      EComResponse openConnection(char *) override;

      /*!\brief Close this layer
        *
        * Implementations of this function should perform the actions necessary for closing.
        */
      void closeConnection() override;

      /*!\brief Take the given data and perform the necessary process for sending
        *
        * If necessary invoke bottom layer sendData functions.
        *
        * @param paData pointer to the data to be sent
        * @param paSize size of the data to be sent
        * @return status of the sending process:
        *    - e_ProcessDataOk ... if sending process was successful
        */
      EComResponse sendData(void *, unsigned int ) override;

      /*!\brief Finish to process the data received in a context outside the communication interrupt i.e. within the event chain of the ComFB.
        *
        * This function shall be used for finishing the data reception.
        */
      EComResponse processInterrupt() override;

      EComResponse recvData(const void *paData, unsigned int paSize) override;
  };

} //forte::com_infra

#endif // _MANUAL_COMLAYER_H_
