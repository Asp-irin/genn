//--------------------------------------------------------------------------
/*! \file pre_wu_vars_in_sim_code/model.cc

\brief model definition file that is part of the feature testing
suite of minimal models with known analytic outcomes that are used for continuous integration testing.
*/
//--------------------------------------------------------------------------


#include "modelSpec.h"

//----------------------------------------------------------------------------
// PreNeuron
//----------------------------------------------------------------------------
class PreNeuron : public NeuronModels::Base
{
public:
    DECLARE_MODEL(PreNeuron, 0, 0);

    SET_THRESHOLD_CONDITION_CODE("$(t) >= (scalar)$(id) && fmodf($(t) - (scalar)$(id), 10.0f)< 1e-4");
    SET_NEEDS_AUTO_REFRACTORY(false);
};

IMPLEMENT_MODEL(PreNeuron);

//----------------------------------------------------------------------------
// PostNeuron
//----------------------------------------------------------------------------
class PostNeuron : public NeuronModels::Base
{
public:
    DECLARE_MODEL(PostNeuron, 0, 0);

    SET_THRESHOLD_CONDITION_CODE("true");
    SET_NEEDS_AUTO_REFRACTORY(false);
};

IMPLEMENT_MODEL(PostNeuron);

//----------------------------------------------------------------------------
// WeightUpdateModel
//----------------------------------------------------------------------------
class WeightUpdateModel : public WeightUpdateModels::Base
{
public:
    DECLARE_WEIGHT_UPDATE_MODEL(WeightUpdateModel, 0, 1, 2, 0);

    SET_VARS({{"w", "scalar"}});
    SET_PRE_VARS({{"s", "scalar"}, {"p", "scalar", VarAccess::READ_ONLY_SHARED_NEURON}});

    SET_SIM_CODE("$(w)= $(s) * $(p);");
    SET_PRE_SPIKE_CODE("$(s) = $(t);\n");
};

IMPLEMENT_MODEL(WeightUpdateModel);

void modelDefinition(ModelSpec &model)
{
#ifdef CL_HPP_TARGET_OPENCL_VERSION
    if(std::getenv("OPENCL_DEVICE") != nullptr) {
        GENN_PREFERENCES.deviceSelectMethod = DeviceSelect::MANUAL;
        GENN_PREFERENCES.manualDeviceID = std::atoi(std::getenv("OPENCL_DEVICE"));
    }
    if(std::getenv("OPENCL_PLATFORM") != nullptr) {
        GENN_PREFERENCES.manualPlatformID = std::atoi(std::getenv("OPENCL_PLATFORM"));
    }
#endif
    model.setDT(1.0);
    model.setName("pre_wu_vars_in_sim_code");

    model.addNeuronPopulation<PreNeuron>("pre", 10, {}, {});
    model.addNeuronPopulation<PostNeuron>("post", 10, {}, {});

    model.addSynapsePopulation<WeightUpdateModel, PostsynapticModels::DeltaCurr>(
        "syn", SynapseMatrixType::SPARSE_INDIVIDUALG, 20, "pre", "post",
        {}, WeightUpdateModel::VarValues(0.0), WeightUpdateModel::PreVarValues(std::numeric_limits<float>::lowest(), 1.0), {},
        {}, {},
        initConnectivity<InitSparseConnectivitySnippet::OneToOne>({}));

    model.setPrecision(GENN_FLOAT);
}
