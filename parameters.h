#ifndef _PARAMETERS_H
#define _PARAMETERS_H

/*****************************************************************************
 * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
 *****************************************************************************/

#include "v2gtp.h"
#include "EXITypes.h"
#include "appHandEXIDatatypes.h"
#include "appHandEXIDatatypesEncoder.h"
#include "appHandEXIDatatypesDecoder.h"
#include "iso1EXIDatatypesEncoder.h"
#include "iso1EXIDatatypesDecoder.h"

static const iso1EnergyTransferModeType dcmode = iso1EnergyTransferModeType_DC_extended;

static const struct iso1DC_EVSEChargeParameterType dccharge =
{
  .DC_EVSEStatus = 0,
  .EVSEMaximumCurrentLimit = {
    .Unit = iso1unitSymbolType_A,
    .Multiplier = 0,
    .Value = 3,
  },
  .EVSEMaximumPowerLimit = {
    .Unit = iso1unitSymbolType_W,
    .Multiplier = 3,
    .Value = 2,
  },
  .EVSEMaximumVoltageLimit = {
    .Unit = iso1unitSymbolType_V,
    .Multiplier = 2,
    .Value = 9,
  },
  .EVSEMinimumCurrentLimit = {
    .Unit = iso1unitSymbolType_A,
    .Multiplier = 0,
    .Value = 0,
  },
  .EVSEMinimumVoltageLimit = {
    .Unit = iso1unitSymbolType_V,
    .Multiplier = 1,
    .Value = 15,
  },
  .EVSECurrentRegulationTolerance_isUsed = 0,
  .EVSEPeakCurrentRipple = {
    .Unit = iso1unitSymbolType_A,
    .Multiplier = 0,
    .Value = 0,
  },
  .EVSEEnergyToBeDelivered_isUsed = 0,
};

static const char iso1string[] = "urn:iso:15118:2:2013:MsgDef";

static const uint16_t max_delay = 12;

#endif

