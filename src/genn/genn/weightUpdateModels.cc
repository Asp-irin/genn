#include "weightUpdateModels.h"

// GeNN includes
#include "gennUtils.h"

using namespace GeNN;

namespace GeNN::WeightUpdateModels
{
IMPLEMENT_SNIPPET(StaticPulse);
IMPLEMENT_SNIPPET(StaticPulseConstantWeight);
IMPLEMENT_SNIPPET(StaticPulseDendriticDelay);
IMPLEMENT_SNIPPET(StaticGraded);
IMPLEMENT_SNIPPET(PiecewiseSTDP);

//----------------------------------------------------------------------------
// GeNN::WeightUpdateModels::Base
//----------------------------------------------------------------------------
boost::uuids::detail::sha1::digest_type Base::getHashDigest() const
{
    // Superclass
    boost::uuids::detail::sha1 hash;
    Models::Base::updateHash(hash);

    Utils::updateHash(getSimCode(), hash);
    Utils::updateHash(getEventCode(), hash);
    Utils::updateHash(getLearnPostCode(), hash);
    Utils::updateHash(getSynapseDynamicsCode(), hash);
    Utils::updateHash(getEventThresholdConditionCode(), hash);
    Utils::updateHash(getPreSpikeCode(), hash);
    Utils::updateHash(getPostSpikeCode(), hash);
    Utils::updateHash(getPreDynamicsCode(), hash);
    Utils::updateHash(getPostDynamicsCode(), hash);
    Utils::updateHash(getPreVars(), hash);
    Utils::updateHash(getPostVars(), hash);

    // Return digest
    return hash.get_digest();
}
//----------------------------------------------------------------------------
boost::uuids::detail::sha1::digest_type Base::getPreHashDigest() const
{
    // Superclass
    // **NOTE** we skip over Models::Base::updateHash to avoid hashing synaptic variables
    boost::uuids::detail::sha1 hash;
    Snippet::Base::updateHash(hash);

    Utils::updateHash(getPreSpikeCode(), hash);
    Utils::updateHash(getPreDynamicsCode(), hash);
    Utils::updateHash(getPreVars(), hash);

    // Return digest
    return hash.get_digest();
}
//----------------------------------------------------------------------------
boost::uuids::detail::sha1::digest_type Base::getPostHashDigest() const
{
    // Superclass
    // **NOTE** we skip over Models::Base::updateHash to avoid hashing synaptic variables
    boost::uuids::detail::sha1 hash;
    Snippet::Base::updateHash(hash);

    Utils::updateHash(getPostSpikeCode(), hash);
    Utils::updateHash(getPostDynamicsCode(), hash);
    Utils::updateHash(getPostVars(), hash);

    // Return digest
    return hash.get_digest();
}
//----------------------------------------------------------------------------
void Base::validate(const std::unordered_map<std::string, double> &paramValues, 
                    const std::unordered_map<std::string, InitVarSnippet::Init> &varValues,
                    const std::unordered_map<std::string, InitVarSnippet::Init> &preVarValues,
                    const std::unordered_map<std::string, InitVarSnippet::Init> &postVarValues,
                    const std::string &description) const
{
    // Superclass
    Models::Base::validate(paramValues, varValues, description);

    Utils::validateVecNames(getPreVars(), "Presynaptic variable");
    Utils::validateVecNames(getPostVars(), "Presynaptic variable");

    // If any variables have a reduction access mode, give an error
    const auto vars = getVars();
    const auto preVars = getPreVars();
    const auto postVars = getPostVars();
    if(std::any_of(vars.cbegin(), vars.cend(),
                   [](const Models::Base::Var &v){ return (v.access & VarAccessModeAttribute::REDUCE); })
       || std::any_of(preVars.cbegin(), preVars.cend(),
                      [](const Models::Base::Var &v){ return (v.access & VarAccessModeAttribute::REDUCE); })
       || std::any_of(postVars.cbegin(), postVars.cend(),
                      [](const Models::Base::Var &v){ return (v.access & VarAccessModeAttribute::REDUCE); }))
    {
        throw std::runtime_error("Weight update models cannot include variables with REDUCE access modes - they are only supported by custom update models");
    }

    // Validate variable reference initialisers
    Utils::validateInitialisers(preVars, preVarValues, "presynaptic variable", description);

    // Validate variable reference initialisers
    Utils::validateInitialisers(postVars, postVarValues, "postsynaptic variable", description);

    // If any variables have shared neuron duplication mode, give an error
    if (std::any_of(vars.cbegin(), vars.cend(),
                    [](const Models::Base::Var &v) { return (v.access & VarAccessDuplication::SHARED_NEURON); }))
    {
        throw std::runtime_error("Weight update models cannot include variables with SHARED_NEURON access modes - they are only supported on pre, postsynaptic or neuron variables");
    }
}
}   // namespace WeightUpdateModels