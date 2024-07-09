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

#include "extevhan.h"

namespace forte::com_infra{
  class CComCallback;
}

class CManualHandler : public CExternalEventHandler{
  DECLARE_HANDLER(CManualHandler)
  public:

    void enableHandler() override;

    void disableHandler() override;

    void setPriority(int paPriority) override;

    int getPriority() const override;

    void registerComCallback(CFunctionBlock& paFunctionBlock, forte::com_infra::CComCallback& paComCallback);

  private:
  
};