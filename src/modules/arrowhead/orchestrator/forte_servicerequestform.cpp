/*******************************************************************************
 * Copyright (c) 2018, fortiss GmbH
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *   Jose Cabral - initial implementation
 *******************************************************************************/

#include "forte_servicerequestform.h"
#ifdef FORTE_ENABLE_GENERATED_SOURCE_CPP
#include "forte_servicerequestform_gen.cpp"
#endif

DEFINE_FIRMWARE_DATATYPE(ServiceRequestForm, g_nStringIdServiceRequestForm);

CIEC_ServiceRequestForm::CIEC_ServiceRequestForm() :
    CIEC_STRUCT(g_nStringIdServiceRequestForm, 6, scmElementTypes, scmElementNames, e_APPLICATION + e_CONSTRUCTED + 1) {
}

const CStringDictionary::TStringId CIEC_ServiceRequestForm::scmElementTypes[] = { g_nStringIdArrowheadSystem, g_nStringIdArrowheadCloud,
  g_nStringIdArrowheadService, g_nStringIdARRAY, 10, g_nStringIdWSTRING, g_nStringIdARRAY, 10, g_nStringIdPreferredProvider, g_nStringIdARRAY, 10,
  g_nStringIdWSTRING };
const CStringDictionary::TStringId CIEC_ServiceRequestForm::scmElementNames[] = { g_nStringIdrequesterSystem, g_nStringIdrequesterCloud,
  g_nStringIdrequestedService, g_nStringIdorchestrationFlags, g_nStringIdpreferredProviders, g_nStringIdrequestedQoS };
