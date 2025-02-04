// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "BitDepthUtils.h"
#include "ops/Gamma/GammaOpData.h"
#include "ops/Matrix/MatrixOps.h"
#include "ops/Range/RangeOpData.h"
#include "ParseUtils.h"
#include "Platform.h"


OCIO_NAMESPACE_ENTER
{

namespace
{

const int FLOAT_DECIMALS = 7;

// Declare the values for an identity operation.
const double IdentityScale = 1.;
const double IdentityOffset = 0.;

// Check if params corresponds to a basic identity.
inline bool IsBasicIdentity(const GammaOpData::Params & p)
{
    return (p[0] == IdentityScale);
}

// Check if params corresponds to a moncurve identity.
inline bool IsMonCurveIdentity(const GammaOpData::Params & p)
{
    return (p[0] == IdentityScale && p[1] == IdentityOffset);
}

std::string GetParametersString(const GammaOpData::Params & params)
{
    std::ostringstream oss;
    oss.precision(FLOAT_DECIMALS);
    oss << params[0];
    for(size_t idx=1; idx<params.size(); ++idx)
    {
        oss << ", " << params[idx];
    }
    return oss.str();
}

static constexpr const char * GAMMA_STYLE_BASIC_FWD    = "basicFwd";
static constexpr const char * GAMMA_STYLE_BASIC_REV    = "basicRev";
static constexpr const char * GAMMA_STYLE_MONCURVE_FWD = "moncurveFwd";
static constexpr const char * GAMMA_STYLE_MONCURVE_REV = "moncurveRev";

}; // anon

GammaOpData::Style GammaOpData::ConvertStringToStyle(const char * str)
{
    if (str && *str)
    {
        if (0 == Platform::Strcasecmp(str, GAMMA_STYLE_BASIC_FWD))
        {
            return BASIC_FWD;
        }
        else if (0 == Platform::Strcasecmp(str, GAMMA_STYLE_BASIC_REV))
        {
            return BASIC_REV;
        }
        else if (0 == Platform::Strcasecmp(str, GAMMA_STYLE_MONCURVE_FWD))
        {
            return MONCURVE_FWD;
        }
        else if (0 == Platform::Strcasecmp(str, GAMMA_STYLE_MONCURVE_REV))
        {
            return MONCURVE_REV;
        }

        std::ostringstream os;
        os << "Unknown gamma style: '" << str << "'.";

        throw Exception(os.str().c_str());
    }

    throw Exception("Missing gamma style.");
}

const char * GammaOpData::ConvertStyleToString(Style style)
{
    switch(style)
    {
        case BASIC_FWD:
            return GAMMA_STYLE_BASIC_FWD;
        case BASIC_REV:
            return GAMMA_STYLE_BASIC_REV;
        case MONCURVE_FWD:
            return GAMMA_STYLE_MONCURVE_FWD;
        case MONCURVE_REV:
            return GAMMA_STYLE_MONCURVE_REV;
    }

    std::stringstream ss("Unknown Gamma style: ");
    ss << style;

    throw Exception(ss.str().c_str());
}

GammaOpData::GammaOpData()
    :   OpData()
    ,   m_style(BASIC_FWD)
    ,   m_redParams(getIdentityParameters(m_style))
    ,   m_greenParams(getIdentityParameters(m_style))
    ,   m_blueParams(getIdentityParameters(m_style))
    ,   m_alphaParams(getIdentityParameters(m_style))
{
}

GammaOpData::GammaOpData(const Style & style,
                         const Params & redParams,
                         const Params & greenParams,
                         const Params & blueParams,
                         const Params & alphaParams)
    :   OpData()
    ,   m_style(style)
    ,   m_redParams(redParams)
    ,   m_greenParams(greenParams)
    ,   m_blueParams(blueParams)
    ,   m_alphaParams(alphaParams)
{
}

GammaOpData::~GammaOpData()
{
}

GammaOpDataRcPtr GammaOpData::clone() const
{
    return std::make_shared<GammaOpData>(*this);
}

GammaOpDataRcPtr GammaOpData::inverse() const
{
    GammaOpDataRcPtr gamma = clone();

    Style invStyle = BASIC_FWD;
    switch(getStyle())
    {
      case BASIC_FWD:     invStyle = BASIC_REV;     break;
      case BASIC_REV:     invStyle = BASIC_FWD;     break;
      case MONCURVE_FWD:  invStyle = MONCURVE_REV;  break;
      case MONCURVE_REV:  invStyle = MONCURVE_FWD;  break;
    }
    gamma->setStyle(invStyle);

    // Note that any existing metadata could become stale at this point but
    // trying to update it is also challenging since inverse() is sometimes
    // called even during the creation of new ops.
    return gamma;
}

bool GammaOpData::isInverse(const GammaOpData & B) const
{
    const Style styleA = getStyle();
    const Style styleB = B.getStyle();

    // Note: It's possible that someone could create something where they
    // don't respect our convention of keeping gamma > 1, in which case,
    // there could be two BASIC_FWD that would be an identity.
    // This code does not to try and handle that case yet.

    if ( (styleA == BASIC_FWD && styleB == BASIC_REV) ||
         (styleA == BASIC_REV && styleB == BASIC_FWD) ||
         (styleA == MONCURVE_FWD && styleB == MONCURVE_REV) ||
         (styleA == MONCURVE_REV && styleB == MONCURVE_FWD) )
    {
        if ( (getRedParams() == B.getRedParams()) &&
             (getGreenParams() == B.getGreenParams()) &&
             (getBlueParams() == B.getBlueParams()) &&
             (getAlphaParams() == B.getAlphaParams()) )
        {
            return true;
        }
    }

    return false;
}

void GammaOpData::setStyle(const Style & style)
{
    // NB: Must call validate after using this method.
    m_style = style;
}

void GammaOpData::setRedParams(const Params & p)
{
    // NB: Must call validate after using this method.
    m_redParams = p;
}

void GammaOpData::setGreenParams(const Params & p)
{
    // NB: Must call validate after using this method.
    m_greenParams = p;
}

void GammaOpData::setBlueParams(const Params & p)
{
    // NB: Must call validate after using this method.
    m_blueParams = p;
}

void GammaOpData::setAlphaParams(const Params & p)
{
    // NB: Must call validate after using this method.
    m_alphaParams = p;
}

void GammaOpData::setParams(const Params & p)
{
    // NB: Must call validate after using this method.
    m_redParams   = p;
    m_greenParams = p;
    m_blueParams  = p;
    m_alphaParams = GammaOpData::getIdentityParameters(getStyle());
}

void validateParams(const GammaOpData::Params & p, 
                    unsigned int reqdSize,
                    const double * lowBounds, 
                    const double * highBounds)
{
    if (p.size() != reqdSize)
    {
        throw Exception("GammaOp: Wrong number of parameters");
    }
    for (unsigned int i=0; i<reqdSize; i++)
    {
        if (p[i] < lowBounds[i])
        {
            std::stringstream ss;
            ss << "Parameter " << p[i] << " is less than lower bound " << lowBounds[i];
            throw Exception(ss.str().c_str());
        }
        if (p[i] > highBounds[i])
        {
            std::stringstream ss;
            ss << "Parameter " << p[i] << " is greater than upper bound " << highBounds[i];
            throw Exception(ss.str().c_str());
        }
    }
}

void GammaOpData::validate() const
{
    OpData::validate();
    GammaOpData::validateParameters();
}

void GammaOpData::validateParameters() const
{
    // Note: When loading from a CTF we want to enforce 
    //       the canonical bounds on the parameters. 

    switch(getStyle())
    {
        case BASIC_FWD:
        case BASIC_REV:
        {
            static const unsigned int reqdSize = 1;
            static const double lowBounds[] = {0.01};
            static const double highBounds[] = {100.};
            validateParams(m_redParams,   reqdSize, lowBounds, highBounds);
            validateParams(m_greenParams, reqdSize, lowBounds, highBounds);
            validateParams(m_blueParams,  reqdSize, lowBounds, highBounds);
            validateParams(m_alphaParams, reqdSize, lowBounds, highBounds);
            break;
        }

        case MONCURVE_FWD:
        case MONCURVE_REV:
        {
            static const unsigned int reqdSize = 2;
            static const double lowBounds[] = {1., 0.};
            static const double highBounds[] = {10., 0.9};
            validateParams(m_redParams,   reqdSize, lowBounds, highBounds);
            validateParams(m_greenParams, reqdSize, lowBounds, highBounds);
            validateParams(m_blueParams,  reqdSize, lowBounds, highBounds);
            validateParams(m_alphaParams, reqdSize, lowBounds, highBounds);
            break;
        }
    }
}

GammaOpData::Params GammaOpData::getIdentityParameters(Style style)
{
    GammaOpData::Params params;
    switch(style)
    {
        case BASIC_FWD:
        case BASIC_REV:
        {
            params.push_back(IdentityScale);
            break;
        }
        case MONCURVE_FWD:
        case MONCURVE_REV:
        {
            params.push_back(IdentityScale);
            params.push_back(IdentityOffset);
            break;
        }
    }

    return params;
}

bool GammaOpData::isIdentityParameters(const Params & parameters, Style style)
{
    switch(style)
    {
        case BASIC_FWD:
        case BASIC_REV:
        {
            return (parameters.size() == 1 && IsBasicIdentity(parameters));
        }
        case MONCURVE_FWD:
        case MONCURVE_REV:
        {
            return (parameters.size() == 2 && IsMonCurveIdentity(parameters));
        }
    }

    return false;
}

bool GammaOpData::isAlphaComponentIdentity() const
{
    return isIdentityParameters(m_alphaParams, getStyle());
}

bool GammaOpData::areAllComponentsEqual() const
{
    // Comparing floats is generally not a good idea, but in this case
    // it is ok to be strict.  Since the same operations are applied to
    // all components, if they started equal, they should remain equal.
    return m_redParams == m_greenParams  &&  m_redParams == m_blueParams 
        && m_redParams == m_alphaParams;
}

bool GammaOpData::isNonChannelDependent() const
{
    return m_redParams == m_greenParams && m_redParams == m_blueParams
        && isAlphaComponentIdentity();
}

bool GammaOpData::isNoOp() const
{
    return isIdentity() && !isClamping();
}

bool GammaOpData::isIdentity() const
{
    switch(getStyle())
    {
        case BASIC_FWD:
        case BASIC_REV:
        {
            if (areAllComponentsEqual() && IsBasicIdentity(m_redParams) )
            {
                return true;
            }
            break;
        }

        case MONCURVE_FWD:
        case MONCURVE_REV:
        {
            if (areAllComponentsEqual() && IsMonCurveIdentity(m_redParams) )
            {
                return true;
            }
            break;
        }
    }
    return false;
}

bool GammaOpData::isClamping() const
{
    return getStyle()==BASIC_FWD || getStyle()==BASIC_REV;
}

bool GammaOpData::mayCompose(const GammaOpData & B) const
{
    // TODO: This hits the most likely scenario, but there are other cases
    //       which technically could be combined (e.g. R & G params unequal).

    // Note: Decided not to make this dependent upon bit depth.

    // NB: This also does not check bypass or dynamic.

    if ( ! isNonChannelDependent() )
        return false;
    
    // At this point, we have R == G == B, and A == identity.

    if (getStyle() != BASIC_FWD && getStyle() != BASIC_REV)
        return false;

    if (B.getStyle() != BASIC_FWD && B.getStyle() != BASIC_REV)
        return false;

    return true;
}

OpDataRcPtr GammaOpData::getIdentityReplacement() const
{
    OpDataRcPtr op;

    switch(getStyle())
    {
        // These clamp values below 0 -- replace with range.
        case BASIC_FWD:
        case BASIC_REV:
        {
            op = std::make_shared<RangeOpData>(0.,
                                               RangeOpData::EmptyValue(), // Don't clamp high end.
                                               0.,
                                               RangeOpData::EmptyValue());

            break;
        }

        // These pass through the full range of values -- replace with matrix.
        case MONCURVE_FWD:
        case MONCURVE_REV:
        {
            op = std::make_shared<MatrixOpData>();
            break;
        }
    }
    op->getFormatMetadata() = getFormatMetadata();
    return op;
}

GammaOpDataRcPtr GammaOpData::compose(const GammaOpData & B) const
{
    if ( ! mayCompose(B) )
    {
        throw Exception("GammaOp can only be combined with some GammaOps");
    }

    // At this point, we have R == G == B, and A == identity
    // and the style is either BASIC FWD or REV.

    double g1 = getRedParams()[0];
    if (getStyle() == BASIC_REV)
    {
        g1 = 1. / g1;
    }

    double g2 = B.getRedParams()[0];
    if (B.getStyle() == BASIC_REV)
    {
        g2 = 1. / g2;
    }

    double gOut = g1 * g2;
    Style style = BASIC_FWD;
    // By convention, we try to keep the gamma parameter > 1.
    if (gOut < 1.0)
    {
        gOut = 1. / gOut;
        style = BASIC_REV;
    }

    // Prevent small rounding errors from not making an identity.
    // E.g., 1/0.45 * 0.45 should have a value exactly 1.
    const double diff = fabs( gOut - 1. );
    if ( diff < 1e-6 )
    {
        gOut = 1.;
    }

    GammaOpData::Params params;
    params.push_back(gOut);
    GammaOpData::Params paramsA;
    paramsA.push_back(1.);

    GammaOpDataRcPtr outOp = std::make_shared<GammaOpData>(style,
                                                           params,
                                                           params,
                                                           params,
                                                           paramsA);

    // TODO: May want to revisit how the metadata is set.
    outOp->getFormatMetadata() = getFormatMetadata();
    outOp->getFormatMetadata().combine(B.getFormatMetadata());

    return outOp;
}

bool GammaOpData::operator==(const OpData & other) const
{
    if(this==&other) return true;

    if(!OpData::operator==(other)) return false;

    const GammaOpData* gop = static_cast<const GammaOpData*>(&other);

    return  m_style == gop->m_style &&
            m_redParams == gop->m_redParams &&
            m_greenParams == gop->m_greenParams &&
            m_blueParams == gop->m_blueParams &&
            m_alphaParams == gop->m_alphaParams;
}

void GammaOpData::finalize()
{
    AutoMutex lock(m_mutex);

    validate();

    std::ostringstream cacheIDStream;
    cacheIDStream << getID() << " ";

    cacheIDStream << GammaOpData::ConvertStyleToString(getStyle()) << " ";

    cacheIDStream << "r:" << GetParametersString(getRedParams())   << " ";
    cacheIDStream << "g:" << GetParametersString(getGreenParams()) << " ";
    cacheIDStream << "b:" << GetParametersString(getBlueParams())  << " ";
    cacheIDStream << "a:" << GetParametersString(getAlphaParams()) << " ";

    m_cacheID = cacheIDStream.str();
}


}
OCIO_NAMESPACE_EXIT


///////////////////////////////////////////////////////////////////////////////

#ifdef OCIO_UNIT_TEST


namespace OCIO = OCIO_NAMESPACE;
#include "UnitTest.h"


OCIO_ADD_TEST(GammaOpData, accessors)
{
    const OCIO::GammaOpData::Params paramsR = { 2.4, 0.1 };
    const OCIO::GammaOpData::Params paramsG = { 2.2, 0.2 };
    const OCIO::GammaOpData::Params paramsB = { 2.0, 0.4 };
    const OCIO::GammaOpData::Params paramsA = { 1.8, 0.6 };

    OCIO::GammaOpData g1(OCIO::GammaOpData::MONCURVE_FWD,
                         paramsR, paramsG, paramsB, paramsA);

    OCIO_CHECK_EQUAL(g1.getType(), OCIO::OpData::GammaType);

    OCIO_CHECK_ASSERT(g1.getRedParams()   == paramsR);
    OCIO_CHECK_ASSERT(g1.getGreenParams() == paramsG);
    OCIO_CHECK_ASSERT(g1.getBlueParams()  == paramsB);
    OCIO_CHECK_ASSERT(g1.getAlphaParams() == paramsA);

    OCIO_CHECK_EQUAL(g1.getStyle(), OCIO::GammaOpData::MONCURVE_FWD);

    OCIO_CHECK_ASSERT( ! g1.areAllComponentsEqual() );
    OCIO_CHECK_ASSERT( ! g1.isNonChannelDependent() );
    OCIO_CHECK_ASSERT( ! g1.isAlphaComponentIdentity() );


    // Set R, G and B params to paramsR, A set to identity.
    g1.setParams(paramsR);

    OCIO_CHECK_ASSERT(!g1.areAllComponentsEqual());
    OCIO_CHECK_ASSERT(g1.isNonChannelDependent());
    OCIO_CHECK_ASSERT(g1.isAlphaComponentIdentity());

    OCIO_CHECK_ASSERT(g1.getGreenParams() == paramsR);
    OCIO_CHECK_ASSERT(
        OCIO::GammaOpData::isIdentityParameters(g1.getAlphaParams(), 
                                                g1.getStyle()));

    g1.setAlphaParams(paramsR);
    OCIO_CHECK_ASSERT(g1.areAllComponentsEqual());

    g1.setBlueParams(paramsB);
    OCIO_CHECK_ASSERT(g1.getBlueParams() == paramsB);

    OCIO_CHECK_ASSERT(!g1.areAllComponentsEqual());

    g1.setRedParams(paramsB);
    OCIO_CHECK_ASSERT(g1.getRedParams() == paramsB);

    g1.setGreenParams(paramsB);
    OCIO_CHECK_ASSERT(g1.getGreenParams() == paramsB);

    g1.setAlphaParams(paramsA);
    OCIO_CHECK_ASSERT(g1.getAlphaParams() == paramsA);

    g1.setStyle(OCIO::GammaOpData::MONCURVE_REV);
    OCIO_CHECK_EQUAL(g1.getStyle(), OCIO::GammaOpData::MONCURVE_REV);
}

OCIO_ADD_TEST(GammaOpData, identity_style_basic)
{
    const OCIO::GammaOpData::Params IdentityParams
        = OCIO::GammaOpData::getIdentityParameters(OCIO::GammaOpData::BASIC_FWD);

    {
        //
        // Basic identity gamma.
        //
        OCIO::GammaOpData g(OCIO::GammaOpData::BASIC_FWD,
                            IdentityParams, IdentityParams,
                            IdentityParams, IdentityParams);
        OCIO_CHECK_ASSERT(g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp()); // inBitDepth != outBitDepth
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

    {
        //
        // Default constructor test:
        // gamma op is BASIC_FWD, in/out bit depth 32f.
        //
        OCIO::GammaOpData g;
        g.setParams(IdentityParams);
        g.validate();
        OCIO_CHECK_EQUAL(g.getStyle(), OCIO::GammaOpData::BASIC_FWD);
        OCIO_CHECK_ASSERT(g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp()); // inBitDepth != outBitDepth
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

    const OCIO::GammaOpData::Params paramsR = { 1.2 };
    const OCIO::GammaOpData::Params paramsG = { 1.6 };
    const OCIO::GammaOpData::Params paramsB = { 2.0 };
    const OCIO::GammaOpData::Params paramsA = { 3.1 };

    {
        //
        // Non-identity check for basic style.
        //
        OCIO::GammaOpData g(OCIO::GammaOpData::BASIC_FWD,
                            paramsR, paramsG, paramsB, paramsA);
        OCIO_CHECK_ASSERT(!g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

    {
        //
        // Non-identity check for default constructor.
        // Default gamma op is BASIC_FWD, in/out bitDepth 32f.
        //
        OCIO::GammaOpData g;
        OCIO_CHECK_ASSERT(g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp()); // basic style clamps, so it isn't a no-op
        OCIO_CHECK_ASSERT(g.isChannelIndependent());

        g.setParams(paramsR);
        g.validate();

        OCIO_CHECK_EQUAL(g.getStyle(), OCIO::GammaOpData::BASIC_FWD);
        OCIO_CHECK_ASSERT(!g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }
}

OCIO_ADD_TEST(GammaOpData, identity_style_moncurve)
{
    const OCIO::GammaOpData::Params IdentityParams
      = OCIO::GammaOpData::getIdentityParameters(OCIO::GammaOpData::MONCURVE_FWD);

    {
        //
        // Identity test for moncurve.
        //
        OCIO::GammaOpData g(OCIO::GammaOpData::MONCURVE_FWD,
                            IdentityParams, IdentityParams,
                            IdentityParams, IdentityParams);
        OCIO_CHECK_ASSERT(g.isIdentity());
        OCIO_CHECK_ASSERT(g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

    {
        //
        // Identity test for forward moncurve with default constructor.
        // Default gamma op is BASIC_FWD, in/out bitDepth 32f.
        //
        OCIO::GammaOpData g;
        g.setStyle(OCIO::GammaOpData::MONCURVE_FWD);
        g.setParams(IdentityParams);
        g.validate();
        OCIO_CHECK_ASSERT(g.isIdentity());
        OCIO_CHECK_ASSERT(g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

    const OCIO::GammaOpData::Params paramsR = { 1.2, 0.2 };
    const OCIO::GammaOpData::Params paramsG = { 1.6, 0.7 };
    const OCIO::GammaOpData::Params paramsB = { 2.0, 0.5 };
    const OCIO::GammaOpData::Params paramsA = { 3.1, 0.1 };

    {
        //
        // Non-identity test for moncurve.
        //
        OCIO::GammaOpData g(OCIO::GammaOpData::MONCURVE_FWD,
                            paramsR, paramsG, paramsB, paramsA);
        OCIO_CHECK_ASSERT(!g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

    {
        //
        // Non-identity test for moncurve with default constructor.
        // Default gamma op is BASIC_FWD, in/out bitDepth 32f.
        //
        OCIO::GammaOpData g;
        g.setStyle(OCIO::GammaOpData::MONCURVE_FWD);
        g.setParams(paramsR);
        g.validate();

        OCIO_CHECK_ASSERT(!g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }
}

OCIO_ADD_TEST(GammaOpData, noop_style_basic)
{
    // Test basic gamma
    const OCIO::GammaOpData::Params IdentityParams
        = OCIO::GammaOpData::getIdentityParameters(OCIO::GammaOpData::BASIC_FWD);

    {
        //
        // NoOp test, basic style.
        //
        OCIO::GammaOpData g(OCIO::GammaOpData::BASIC_FWD,
                            IdentityParams, IdentityParams,
                            IdentityParams, IdentityParams);
        OCIO_CHECK_ASSERT(g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp()); // basic style clamps, so it isn't a no-op
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

    const OCIO::GammaOpData::Params paramsR = { 1.2 };
    const OCIO::GammaOpData::Params paramsG = { 1.6 };
    const OCIO::GammaOpData::Params paramsB = { 2.0 };
    const OCIO::GammaOpData::Params paramsA = { 3.1 };

    {
        //
        // Non-NoOp test, basic style.
        //
        OCIO::GammaOpData g(OCIO::GammaOpData::BASIC_FWD,
                            paramsR, paramsG, paramsB, paramsA);
        OCIO_CHECK_ASSERT(!g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

}

OCIO_ADD_TEST(GammaOpData, noop_style_moncurve)
{
    // Test monCurve gamma
    const OCIO::GammaOpData::Params IdentityParams
        = OCIO::GammaOpData::getIdentityParameters(OCIO::GammaOpData::MONCURVE_FWD);

    {
        //
        // NoOp test, moncurve style.
        //
        OCIO::GammaOpData g(OCIO::GammaOpData::MONCURVE_FWD,
                            IdentityParams, IdentityParams,
                            IdentityParams, IdentityParams);
        OCIO_CHECK_ASSERT(g.isIdentity());
        OCIO_CHECK_ASSERT(g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

    const OCIO::GammaOpData::Params paramsR = { 1.2, 0.2 };
    const OCIO::GammaOpData::Params paramsG = { 1.6, 0.7 };
    const OCIO::GammaOpData::Params paramsB = { 2.0, 0.5 };
    const OCIO::GammaOpData::Params paramsA = { 3.1, 0.1 };

    {
        //
        // Non-NoOp test, moncurve style.
        //
        OCIO::GammaOpData g(OCIO::GammaOpData::MONCURVE_FWD,
                            paramsR, paramsG, paramsB, paramsA);
        OCIO_CHECK_ASSERT(!g.isIdentity());
        OCIO_CHECK_ASSERT(!g.isNoOp());
        OCIO_CHECK_ASSERT(g.isChannelIndependent());
    }

}

OCIO_ADD_TEST(GammaOpData, validate)
{
    const OCIO::GammaOpData::Params params = { 2.6 };

    const OCIO::GammaOpData::Params paramsR = { 2.4, 0.1 };
    const OCIO::GammaOpData::Params paramsG = { 2.2, 0.2 };
    const OCIO::GammaOpData::Params paramsB = { 2.0, 0.4 };
    const OCIO::GammaOpData::Params paramsA = { 1.8, 0.6 };

    {
        OCIO::GammaOpData g1(OCIO::GammaOpData::MONCURVE_FWD,
                             paramsR, paramsG, params, paramsA);
        OCIO_CHECK_THROW_WHAT(g1.validate(),
                              OCIO::Exception,
                              "GammaOp: Wrong number of parameters");
    }

    {
        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_FWD,
                             paramsB, paramsB, paramsB, paramsB);
        OCIO_CHECK_THROW_WHAT(g1.validate(),
                              OCIO::Exception,
                              "GammaOp: Wrong number of parameters");
    }

    {
        const OCIO::GammaOpData::Params params1 = { 0.006 }; // valid range is [0.01, 100]

        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_FWD,
                             params1, params1, params1, params1);
        OCIO_CHECK_THROW_WHAT(g1.validate(),
                              OCIO::Exception,
                              "Parameter 0.006 is less than lower bound 0.01");
    }

    {
        const OCIO::GammaOpData::Params params1 = { 110. }; // valid range is [0.01, 100]
        
        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_FWD,
                             params1, params1, params1, params1);
        OCIO_CHECK_THROW_WHAT(g1.validate(),
                              OCIO::Exception,
                              "Parameter 110 is greater than upper bound 100");
    }


    {
        const OCIO::GammaOpData::Params params1 = { 1.,    // valid range is [1, 10]
                                                    11. }; // valid range is [0, 0.9]

        OCIO::GammaOpData g1(OCIO::GammaOpData::MONCURVE_FWD,
                             params1, params1, params1, params1);
        OCIO_CHECK_THROW_WHAT(g1.validate(),
                              OCIO::Exception,
                              "Parameter 11 is greater than upper bound 0.9");
    }

    {
        const OCIO::GammaOpData::Params params1 = { 1.,   // valid range is [1, 10]
                                                    0. }; // valid range is [0, 0.9]

        OCIO::GammaOpData g1(OCIO::GammaOpData::MONCURVE_FWD,
                             params1, params1, params1, params1);

        OCIO_CHECK_NO_THROW( g1.validate() );
    }

    {
        const OCIO::GammaOpData::Params params1 = { 1.,     // valid range is [1, 10]
                                                   -1e-6 }; // valid range is [0, 0.9]

        OCIO::GammaOpData g1(OCIO::GammaOpData::MONCURVE_FWD,
                             params1, params1, params1, params1);
        OCIO_CHECK_THROW_WHAT(g1.validate(),
                              OCIO::Exception,
                              "Parameter -1e-06 is less than lower bound 0");
    }
}

OCIO_ADD_TEST(GammaOpData, equality)
{
    const OCIO::GammaOpData::Params paramsR1 = { 2.4, 0.1 };
    const OCIO::GammaOpData::Params paramsG1 = { 2.2, 0.2 };
    const OCIO::GammaOpData::Params paramsB1 = { 2.0, 0.4 };
    const OCIO::GammaOpData::Params paramsA1 = { 1.8, 0.6 };

    OCIO::GammaOpData g1(OCIO::GammaOpData::MONCURVE_FWD,
                         paramsR1, paramsG1, paramsB1, paramsA1);

    const OCIO::GammaOpData::Params paramsR2 = { 2.6, 0.1 }; // 2.6 != 2.4
    const OCIO::GammaOpData::Params paramsG2 = paramsG1;
    const OCIO::GammaOpData::Params paramsB2 = paramsB1;
    const OCIO::GammaOpData::Params paramsA2 = paramsA1;

    OCIO::GammaOpData g2(OCIO::GammaOpData::MONCURVE_FWD,
                         paramsR2, paramsG2, paramsB2, paramsA2);

    OCIO_CHECK_ASSERT(!(g1 == g2));

    OCIO::GammaOpData g3(OCIO::GammaOpData::MONCURVE_REV,
                         paramsR1, paramsG1, paramsB1, paramsA1);

    OCIO_CHECK_ASSERT(!(g3 == g1));

    g3.setStyle(g1.getStyle());
    g3.validate();

    OCIO_CHECK_ASSERT(g3 == g1);

    OCIO::GammaOpData g4(OCIO::GammaOpData::MONCURVE_FWD,
                         paramsR1, paramsG1, paramsB1, paramsA1);

    OCIO_CHECK_ASSERT(g4 == g1);
}

namespace
{

void CheckGammaInverse(OCIO::GammaOpData::Style refStyle,
                       const OCIO::GammaOpData::Params & refParamsR,
                       const OCIO::GammaOpData::Params & refParamsG,
                       const OCIO::GammaOpData::Params & refParamsB,
                       const OCIO::GammaOpData::Params & refParamsA,
                       OCIO::GammaOpData::Style invStyle,
                       const OCIO::GammaOpData::Params & invParamsR,
                       const OCIO::GammaOpData::Params & invParamsG,
                       const OCIO::GammaOpData::Params & invParamsB,
                       const OCIO::GammaOpData::Params & invParamsA )
{
    OCIO::GammaOpData refGammaOp(refStyle,
                                 refParamsR,
                                 refParamsG,
                                 refParamsB,
                                 refParamsA);

    OCIO::GammaOpDataRcPtr invOp = refGammaOp.inverse();

    OCIO_CHECK_EQUAL(invOp->getStyle(), invStyle);

    OCIO_CHECK_ASSERT(invOp->getRedParams()   == invParamsR);
    OCIO_CHECK_ASSERT(invOp->getGreenParams() == invParamsG);
    OCIO_CHECK_ASSERT(invOp->getBlueParams()  == invParamsB);
    OCIO_CHECK_ASSERT(invOp->getAlphaParams() == invParamsA);

    OCIO_CHECK_ASSERT(refGammaOp.isInverse(*invOp));
    OCIO_CHECK_ASSERT(invOp->isInverse(refGammaOp));
    OCIO_CHECK_ASSERT(!refGammaOp.isInverse(refGammaOp));
    OCIO_CHECK_ASSERT(!invOp->isInverse(*invOp));
}

};

OCIO_ADD_TEST(GammaOpData, basic_inverse)
{
    const OCIO::GammaOpData::Params paramsR = { 2.2 };
    const OCIO::GammaOpData::Params paramsG = { 2.4 };
    const OCIO::GammaOpData::Params paramsB = { 2.6 };
    const OCIO::GammaOpData::Params paramsA = { 2.8 };

    CheckGammaInverse(OCIO::GammaOpData::BASIC_FWD, paramsR, paramsG, paramsB, paramsA,
                      OCIO::GammaOpData::BASIC_REV, paramsR, paramsG, paramsB, paramsA);

    CheckGammaInverse(OCIO::GammaOpData::BASIC_REV, paramsR, paramsG, paramsB, paramsA,
                      OCIO::GammaOpData::BASIC_FWD, paramsR, paramsG, paramsB, paramsA);
}

OCIO_ADD_TEST(GammaOpData, moncurve_inverse)
{
    const OCIO::GammaOpData::Params paramsR = { 2.4, 0.1 };
    const OCIO::GammaOpData::Params paramsG = { 2.2, 0.2 };
    const OCIO::GammaOpData::Params paramsB = { 2.0, 0.4 };
    const OCIO::GammaOpData::Params paramsA = { 1.8, 0.6 };


    CheckGammaInverse(OCIO::GammaOpData::MONCURVE_FWD, paramsR, paramsG, paramsB, paramsA,
                      OCIO::GammaOpData::MONCURVE_REV, paramsR, paramsG, paramsB, paramsA);

    CheckGammaInverse(OCIO::GammaOpData::MONCURVE_REV, paramsR, paramsG, paramsB, paramsA,
                      OCIO::GammaOpData::MONCURVE_FWD, paramsR, paramsG, paramsB, paramsA);
}

OCIO_ADD_TEST(GammaOpData, is_inverse)
{
    // NB: isInverse ignores bit-depth.

    // See also addtl tests in CheckGammaInverse() above.
    // Just need to test that if params are unequal it is not an inverse.
    OCIO::GammaOpData::Params paramsR = { 2.4  };   // gamma
    OCIO::GammaOpData::Params paramsG = { 2.41 };   // gamma

    OCIO::GammaOpData GammaOp1(OCIO::GammaOpData::BASIC_FWD, 
                               paramsR, paramsG, paramsR, paramsR);

    OCIO::GammaOpData GammaOp2(OCIO::GammaOpData::BASIC_REV, 
                               paramsR, paramsG, paramsR, paramsR);

    // Set B param differently.
    OCIO::GammaOpData GammaOp3(OCIO::GammaOpData::BASIC_REV, 
                               paramsR, paramsG, paramsG, paramsR);

    OCIO_CHECK_ASSERT(GammaOp1.isInverse(GammaOp2));
    OCIO_CHECK_ASSERT(!GammaOp1.isInverse(GammaOp3));

    paramsR.push_back(0.1);    // offset
    paramsG.push_back(0.1);    // offset

    OCIO::GammaOpData GammaOp1m(OCIO::GammaOpData::MONCURVE_FWD, 
                                paramsR, paramsG, paramsR, paramsR);

    OCIO::GammaOpData GammaOp2m(OCIO::GammaOpData::MONCURVE_REV, 
                                paramsR, paramsG, paramsR, paramsR);

    // Set blue param differently.
    OCIO::GammaOpData GammaOp3m(OCIO::GammaOpData::MONCURVE_REV, 
                                paramsR, paramsG, paramsG, paramsR);

    OCIO_CHECK_ASSERT(GammaOp1m.isInverse(GammaOp2m));
    OCIO_CHECK_ASSERT(!GammaOp1m.isInverse(GammaOp3m));
}

OCIO_ADD_TEST(GammaOpData, mayCompose)
{
    OCIO::GammaOpData::Params params1 = { 1.  };
    OCIO::GammaOpData::Params params2 = { 2.2 };
    OCIO::GammaOpData::Params params3 = { 2.6 };

    {
        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_FWD,
                             params2, params2, params2, params1);
        OCIO::GammaOpData g2(OCIO::GammaOpData::BASIC_FWD,
                             params2, params2, params2, params1);
        // Note: Bit-depths don't need to match.
        OCIO_CHECK_ASSERT(g1.mayCompose(g2));
    }

    {
        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_FWD,
                             params2, params2, params2, params2);
        OCIO::GammaOpData g2(OCIO::GammaOpData::BASIC_FWD,
                             params2, params2, params2, params2);
        // Non-identity alpha.
        OCIO_CHECK_ASSERT(!g1.mayCompose(g2));
    }

    {
        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_FWD,
                             params2, params2, params2, params1);
        OCIO::GammaOpData g2(OCIO::GammaOpData::BASIC_REV,
                             params3, params3, params3, params1);
        // Basic may be fwd or rev.
        OCIO_CHECK_ASSERT(g1.mayCompose(g2));
    }

    {
        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_FWD,
                             params2, params2, params1, params1);
        OCIO::GammaOpData g2(OCIO::GammaOpData::BASIC_FWD,
                             params2, params2, params2, params1);
        // R == G != B params.
        OCIO_CHECK_ASSERT(!g1.mayCompose(g2));
    }

    {
        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_FWD,
                             params2, params2, params2, params1);
        params1.push_back(0.0);
        params3.push_back(0.1);
        OCIO::GammaOpData g2(OCIO::GammaOpData::MONCURVE_FWD,
                             params3, params3, params3, params1);
        // Moncurve not allowed.
        OCIO_CHECK_ASSERT(!g1.mayCompose(g2));
    }
}

namespace
{

void CheckGammaCompose(OCIO::GammaOpData::Style style1,
                       const OCIO::GammaOpData::Params & params1,
                       OCIO::GammaOpData::Style style2,
                       const OCIO::GammaOpData::Params & params2,
                       OCIO::GammaOpData::Style refStyle,
                       const OCIO::GammaOpData::Params & refParams)
{
    static const OCIO::GammaOpData::Params paramsA = { 1. };

    const OCIO::GammaOpData g1(style1, 
                               params1, params1, params1, paramsA);

    const OCIO::GammaOpData g2(style2, 
                               params2, params2, params2, paramsA);

    const OCIO::GammaOpDataRcPtr g3 = g1.compose(g2);

    OCIO_CHECK_EQUAL(g3->getStyle(), refStyle);

    OCIO_CHECK_ASSERT(g3->getRedParams()   == refParams);
    OCIO_CHECK_ASSERT(g3->getGreenParams() == refParams);
    OCIO_CHECK_ASSERT(g3->getBlueParams()  == refParams);
    OCIO_CHECK_ASSERT(g3->getAlphaParams() == paramsA);
}

};

OCIO_ADD_TEST(GammaOpData, compose)
{
    {
        const OCIO::GammaOpData::Params params1 = { 2. };
        const OCIO::GammaOpData::Params params2 = { 3. };
        const OCIO::GammaOpData::Params refParams = { 6. };

        CheckGammaCompose(OCIO::GammaOpData::BASIC_FWD, params1,
                          OCIO::GammaOpData::BASIC_FWD, params2,
                          OCIO::GammaOpData::BASIC_FWD, refParams);
    }

    {
        const OCIO::GammaOpData::Params params1 = { 2. };
        const OCIO::GammaOpData::Params params2 = { 4. };
        const OCIO::GammaOpData::Params refParams = { 8. };

        CheckGammaCompose(OCIO::GammaOpData::BASIC_REV, params1,
                          OCIO::GammaOpData::BASIC_REV, params2,
                          OCIO::GammaOpData::BASIC_REV, refParams);
    }

    {
        const OCIO::GammaOpData::Params params1 = { 4. };
        const OCIO::GammaOpData::Params params2 = { 2. };
        const OCIO::GammaOpData::Params refParams = { 2. };

        CheckGammaCompose(OCIO::GammaOpData::BASIC_REV, params1,
                          OCIO::GammaOpData::BASIC_FWD, params2,
                          OCIO::GammaOpData::BASIC_REV, refParams);
    }

    {
        const OCIO::GammaOpData::Params params1 = { 2. };
        const OCIO::GammaOpData::Params params2 = { 4. };
        const OCIO::GammaOpData::Params refParams = { 2. };

        CheckGammaCompose(OCIO::GammaOpData::BASIC_REV, params1,
                          OCIO::GammaOpData::BASIC_FWD, params2,
                          OCIO::GammaOpData::BASIC_FWD, refParams);
    }

    {
        const OCIO::GammaOpData::Params params1 = { 4. };
        OCIO::GammaOpData::Params paramsA = { 1. };
        OCIO::GammaOpData g1(OCIO::GammaOpData::BASIC_REV, 
                             params1, params1, params1, paramsA);

        OCIO::GammaOpData::Params params2 = {2., 0.1};
        paramsA.push_back(0.0);

        OCIO::GammaOpData g2(OCIO::GammaOpData::MONCURVE_REV, 
                             params2, params2, params2, paramsA);

        OCIO_CHECK_THROW_WHAT(g1.compose(g2), 
                              OCIO::Exception, 
                              "GammaOp can only be combined with some GammaOps");
    }
}


#endif // OCIO_UNIT_TEST
