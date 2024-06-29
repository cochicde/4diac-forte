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

#include "ecet.h"

#include <string>

class CEcetFactory {
  public:

    static CEventChainExecutionThread* createEcet();

    static void setEcetNameToCreate(std::string paName);

  private:

    static std::string smEcetNameToCreate;

};