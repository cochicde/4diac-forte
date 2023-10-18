/*******************************************************************************
 * Copyright (c) 2023 Primetals Technologies Austria GmbH
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Mario Kastner, Alois Zoitl - initial implementation
 *******************************************************************************/

#pragma once

#include "localcomlayer.h"

namespace forte {

  namespace com_infra {


    /*! A local communication layer that allows to write to a single struct member of a local group.
     *
     * The boundary conditions are that the local group publishing and subscribing is with a single
     * RD/SD of type struct.
     *
     * This layer can only be used with PUBLISH_1 FBs.
     *
     * To use it a ID param with the following structure needs to be provided:
     *    structmemb[localgroupname;structtype;structmembername]
     *
     *  - localgroupname:  is the local group this local com layer should attach to. If this is the
     *                     first block for this group an according group is created.
     *  - structtype: the type of the struct this layer expects in the local group. If the local
     *                group is existing it will be checked if the local group is of size one and
     *                that the data value exchanged is of this size. If no local group is existing
     *                a group with this struct type is created.
     *  - structmembername: the name of a struct member of the above struct type. This is the struct
     *                      member this layer will write to.
     */
    class CStructMemberLocalComLayer : public CLocalComLayer{

      public:
        CStructMemberLocalComLayer(CComLayer *paUpperLayer, CBaseCommFB *paFB);

      protected:
        void setRDs(forte::com_infra::CBaseCommFB &paSubl, CIEC_ANY **paSDs, TPortId paNumSDs);

      private:
        static constexpr size_t scmNumLayerParameters = 3;

        EComResponse openConnection(char *paLayerParameter) override;

        size_t mMemberIndex;
    };
  }
}