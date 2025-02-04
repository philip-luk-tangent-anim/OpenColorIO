// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#ifndef INCLUDED_OCIO_FILEFORMATS_CTF_CTFREADERUTILS_H
#define INCLUDED_OCIO_FILEFORMATS_CTF_CTFREADERUTILS_H

#include <OpenColorIO/OpenColorIO.h>

OCIO_NAMESPACE_ENTER
{
    
Interpolation GetInterpolation1D(const char * str);
const char * GetInterpolation1DName(Interpolation interp);
Interpolation GetInterpolation3D(const char * str);
const char * GetInterpolation3DName(Interpolation interp);

static constexpr const char * TAG_ACES = "ACES";
static constexpr const char * TAG_ACES_PARAMS = "ACESParams";
static constexpr const char * TAG_ARRAY = "Array";
static constexpr const char * TAG_CDL = "ASC_CDL";
static constexpr const char * TAG_DYN_PROP_CONTRAST = "CONTRAST";
static constexpr const char * TAG_DYN_PROP_EXPOSURE = "EXPOSURE";
static constexpr const char * TAG_DYN_PROP_GAMMA = "GAMMA";
static constexpr const char * TAG_DYNAMIC_PARAMETER = "DynamicParameter";
static constexpr const char * TAG_EXPOSURE_CONTRAST = "ExposureContrast";
static constexpr const char * TAG_EC_PARAMS = "ECParams";
static constexpr const char * TAG_FIXED_FUNCTION = "FixedFunction";
static constexpr const char * TAG_GAMMA = "Gamma";
static constexpr const char * TAG_GAMMA_PARAMS = "GammaParams";
static constexpr const char * TAG_INDEX_MAP = "IndexMap";
static constexpr const char * TAG_INFO = "Info";
static constexpr const char * TAG_INVLUT1D = "InverseLUT1D";
static constexpr const char * TAG_INVLUT3D = "InverseLUT3D";
static constexpr const char * TAG_LOG = "Log";
static constexpr const char * TAG_LOG_PARAMS = "LogParams";
static constexpr const char * TAG_LUT1D = "LUT1D";
static constexpr const char * TAG_LUT3D = "LUT3D";
static constexpr const char * TAG_MATRIX = "Matrix";
static constexpr const char * TAG_MAX_IN_VALUE = "maxInValue";
static constexpr const char * TAG_MAX_OUT_VALUE = "maxOutValue";
static constexpr const char * TAG_MIN_IN_VALUE = "minInValue";
static constexpr const char * TAG_MIN_OUT_VALUE = "minOutValue";
static constexpr const char * TAG_PROCESS_LIST = "ProcessList";
static constexpr const char * TAG_RANGE = "Range";
static constexpr const char * TAG_REFERENCE = "Reference";

static constexpr const char * ATTR_ALIAS = "alias";
static constexpr const char * ATTR_BASE = "base";
static constexpr const char * ATTR_BASE_PATH = "basePath";
static constexpr const char * ATTR_BITDEPTH_IN = "inBitDepth";
static constexpr const char * ATTR_BITDEPTH_OUT = "outBitDepth";
static constexpr const char * ATTR_CHAN = "channel";
static constexpr const char * ATTR_COMP_CLF_VERSION = "compCLFversion";
static constexpr const char * ATTR_CONTRAST = "contrast";
static constexpr const char * ATTR_DIMENSION = "dim";
static constexpr const char * ATTR_EXPOSURE = "exposure";
static constexpr const char * ATTR_GAMMA = "gamma";
static constexpr const char * ATTR_HALF_DOMAIN = "halfDomain";
static constexpr const char * ATTR_HIGHLIGHT = "highlight";
static constexpr const char * ATTR_HUE_ADJUST = "hueAdjust";
static constexpr const char * ATTR_INTERPOLATION = "interpolation";
static constexpr const char * ATTR_INVERSE_OF = "inverseOf";
static constexpr const char * ATTR_IS_INVERTED = "inverted";
static constexpr const char * ATTR_LINSIDESLOPE = "linSideSlope";
static constexpr const char * ATTR_LINSIDEOFFSET = "linSideOffset";
static constexpr const char * ATTR_LOGEXPOSURESTEP = "logExposureStep";
static constexpr const char * ATTR_LOGMIDGRAY = "logMidGray";
static constexpr const char * ATTR_LOGSIDESLOPE = "logSideSlope";
static constexpr const char * ATTR_LOGSIDEOFFSET = "logSideOffset";
static constexpr const char * ATTR_NAME = "name";
static constexpr const char * ATTR_OFFSET = "offset";
static constexpr const char * ATTR_PARAM = "param";
static constexpr const char * ATTR_PARAMS = "params";
static constexpr const char * ATTR_PATH = "path";
static constexpr const char * ATTR_PIVOT = "pivot";
static constexpr const char * ATTR_RAW_HALFS = "rawHalfs";
static constexpr const char * ATTR_REFBLACK = "refBlack";
static constexpr const char * ATTR_REFWHITE = "refWhite";
static constexpr const char * ATTR_SHADOW = "shadow";
static constexpr const char * ATTR_STYLE = "style";
static constexpr const char * ATTR_VERSION = "version";

static constexpr const char * LOG_LOG2 = "log2";
static constexpr const char * LOG_LOG10 = "log10";
static constexpr const char * LOG_ANTILOG2 = "antiLog2";
static constexpr const char * LOG_ANTILOG10 = "antiLog10";
static constexpr const char * LOG_LINTOLOG = "linToLog";
static constexpr const char * LOG_LOGTOLIN = "logToLin";

}
OCIO_NAMESPACE_EXIT

#endif