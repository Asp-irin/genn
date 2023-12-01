#include "runtime/runtime.h"

// Standard C++ includes
#include <fstream>
#include <unordered_set>

// PLOG includes
#include <plog/Log.h>

// Filesystem includes
#include "path.h"

// GeNN includes
#include "varAccess.h"

// GeNN code generator includes
#include "code_generator/backendBase.h"
#include "code_generator/modelSpecMerged.h"

using namespace GeNN;
using namespace GeNN::CodeGenerator;

//--------------------------------------------------------------------------
// Anonymous namespace
//--------------------------------------------------------------------------
namespace
{
size_t getNumSynapseVarElements(VarAccessDim varDims, const BackendBase &backend, const SynapseGroupInternal &sg)
{
    if(varDims & VarAccessDim::ELEMENT) {
        if(sg.getMatrixType() & SynapseMatrixWeight::KERNEL) {
            return sg.getKernelSizeFlattened();
        }
        else {
            return sg.getSrcNeuronGroup()->getNumNeurons() * backend.getSynapticMatrixRowStride(sg);
        }
    }
    else {
        return 1;
    }
}
}   // Anonymous namespace

//--------------------------------------------------------------------------
// GeNN::Runtime::ArrayBase
//--------------------------------------------------------------------------
namespace GeNN::Runtime
{
void ArrayBase::memsetHostPointer(int value)
{
    std::memset(m_HostPointer, value, getSizeBytes());
}
//--------------------------------------------------------------------------
void ArrayBase::serialiseHostPointer(std::vector<std::byte> &bytes, bool pointerToPointer) const
{
    std::byte vBytes[sizeof(void*)];
    if(pointerToPointer) {
        std::byte* const *hostPointerPointer = &m_HostPointer;
        std::memcpy(vBytes, &hostPointerPointer, sizeof(void*));
    }
    else {
        std::memcpy(vBytes, &m_HostPointer, sizeof(void*));
    }
    std::copy(std::begin(vBytes), std::end(vBytes), std::back_inserter(bytes));
}

//--------------------------------------------------------------------------
// GeNN::Runtime::Runtime
//--------------------------------------------------------------------------
Runtime::Runtime(const filesystem::path &modelPath, const CodeGenerator::ModelSpecMerged &modelMerged, 
                 const CodeGenerator::BackendBase &backend)
:   m_Timestep(0), m_ModelMerged(modelMerged), m_Backend(backend), m_AllocateMem(nullptr), m_FreeMem(nullptr),
    m_Initialize(nullptr), m_InitializeSparse(nullptr), m_InitializeHost(nullptr), m_StepTime(nullptr)
{

    // Load library
#ifdef _WIN32
    const std::string runnerName = "runner_" + modelMerged.getModel().getName();
    const std::string runnerNameSuffix = backend.getPreferences().debugCode ?  "_Debug.dll" :  "_Release.dll";
    const std::string libraryName = (modelPath / (runnerName + runnerNameSuffix)).str();
    m_Library = LoadLibrary(libraryName.c_str());
#else
    const std::string libraryName = (modelPath / (modelMerged.getModel().getName() + "_CODE") / "librunner.so").str();
    m_Library = dlopen(libraryName.c_str(), RTLD_NOW);
#endif

    // If library was loaded successfully, look up basic functions in library
    if(m_Library != nullptr) {
        m_AllocateMem = (VoidFunction)getSymbol("allocateMem");
        m_FreeMem = (VoidFunction)getSymbol("freeMem");

        m_Initialize = (VoidFunction)getSymbol("initialize");
        m_InitializeSparse = (VoidFunction)getSymbol("initializeSparse");
        m_InitializeHost = (VoidFunction)getSymbol("initializeHost");

        m_StepTime = (StepTimeFunction)getSymbol("stepTime");

        /*m_NCCLGenerateUniqueID = (VoidFunction)getSymbol("ncclGenerateUniqueID", true);
        m_NCCLGetUniqueID = (UCharPtrFunction)getSymbol("ncclGetUniqueID", true);
        m_NCCLInitCommunicator = (NCCLInitCommunicatorFunction)getSymbol("ncclInitCommunicator", true);
        m_NCCLUniqueIDBytes = (unsigned int*)getSymbol("ncclUniqueIDBytes", true);*/

        // Build set of custom update group names
        std::unordered_set<std::string> customUpdateGroupNames;
        std::transform(getModel().getCustomUpdates().cbegin(), getModel().getCustomUpdates().cend(),
                       std::inserter(customUpdateGroupNames, customUpdateGroupNames.end()),
                       [](const auto &v) { return v.second.getUpdateGroupName(); });
        std::transform(getModel().getCustomWUUpdates().cbegin(), getModel().getCustomWUUpdates().cend(),
                       std::inserter(customUpdateGroupNames, customUpdateGroupNames.end()),
                       [](const auto &v) { return v.second.getUpdateGroupName(); });
        std::transform(getModel().getCustomConnectivityUpdates().cbegin(), getModel().getCustomConnectivityUpdates().cend(),
                       std::inserter(customUpdateGroupNames, customUpdateGroupNames.end()),
                       [](const auto &v) { return v.second.getUpdateGroupName(); });

        // Get function pointers to custom update functions for each group
        std::transform(customUpdateGroupNames.cbegin(), customUpdateGroupNames.cend(), 
                       std::inserter(m_CustomUpdateFunctions, m_CustomUpdateFunctions.end()),
                       [this](const auto &n)
                       { 
                           return std::make_pair(n, (CustomUpdateFunction)getSymbol("update" + n)); 
                       });
    }
    else {
#ifdef _WIN32
        throw std::runtime_error("Unable to load library - error:" + std::to_string(GetLastError()));
#else
        throw std::runtime_error("Unable to load library - error:" + std::string(dlerror()));
#endif
    }
}
//----------------------------------------------------------------------------
Runtime::~Runtime()
{
    if(m_Library) {
        m_FreeMem();

#ifdef _WIN32
        FreeLibrary(m_Library);
#else
        dlclose(m_Library);
#endif
        m_Library = nullptr;
    }
}
//----------------------------------------------------------------------------
void Runtime::allocate(std::optional<size_t> numRecordingTimesteps)
{
    // Call allocate function in generated code
    m_AllocateMem();

    // Store number of recording timesteps
    m_NumRecordingTimesteps = numRecordingTimesteps;

    // Loop through neuron groups
    const size_t batchSize = getModel().getBatchSize();
    for(const auto &n : getModel().getNeuronGroups()) {
        LOGD_RUNTIME << "Allocating memory for neuron group '" << n.first << "'";
        const size_t numNeuronDelaySlots = batchSize * n.second.getNumNeurons() * n.second.getNumDelaySlots();
        const size_t numRecordingWords = (ceilDivide(n.second.getNumNeurons(), 32) * batchSize) * numRecordingTimesteps.value();

        // If spike or spike-like event recording is enabled
        if(n.second.isSpikeRecordingEnabled() || n.second.isSpikeEventRecordingEnabled()) {
            if(!numRecordingTimesteps) {
                throw std::runtime_error("Cannot use recording system without specifying number of recording timesteps");
            }

            if(n.second.isSpikeRecordingEnabled()) {
                createArray(&n.second, "recordSpk", Type::Uint32, numRecordingWords, 
                            VarLocation::HOST_DEVICE);
            }
        }

        // If neuron group has axonal or back-propagation delays, add delay queue pointer
        if (n.second.isDelayRequired()) {
            createArray(&n.second, "spkQuePtr", Type::Uint32, 1, VarLocation::DEVICE);
            m_DelayQueuePointer.try_emplace(&n.second, 0);
        }
        
        // If neuron group needs per-neuron RNGs
        if(n.second.isSimRNGRequired()) {
            auto rng = m_Backend.get().createPopulationRNG(batchSize * n.second.getNumNeurons());
            if(rng) {
                const auto r = m_NeuronGroupArrays[&n.second].try_emplace("rng", std::move(rng));
                if(!r.second) {
                    throw std::runtime_error("Unable to allocate array with " 
                                             "duplicate name 'rng'");
                }
            }
        }

        // Create destinations for any dynamic parameters
        createDynamicParamDestinations<NeuronGroupInternal>(n.second, n.second.getNeuronModel()->getParams(),
                                                            &NeuronGroup::isParamDynamic);

        // Create arrays for neuron state variables
        createNeuronVarArrays<NeuronVarAdapter>(&n.second, n.second.getNumNeurons(), 
                                                batchSize, n.second.getNumDelaySlots(), true);
        
        // Create arrays for neuron extra global parameters
        createEGPArrays<NeuronEGPAdapter>(&n.second);

        // Create arrays for current source variables and extra global parameters
        for (const auto *cs : n.second.getCurrentSources()) {
            createNeuronVarArrays<CurrentSourceVarAdapter>(cs, n.second.getNumNeurons(), batchSize, 1, true);
            createEGPArrays<CurrentSourceEGPAdapter>(cs);
            createDynamicParamDestinations<CurrentSourceInternal>(*cs, cs->getCurrentSourceModel()->getParams(),
                                                                  &CurrentSourceInternal::isParamDynamic);
        }

        // Loop through fused postsynaptic model from incoming populations
        for(const auto *sg : n.second.getFusedPSMInSyn()) {
            createArray(sg, "outPost", getModel().getPrecision(), 
                        sg->getTrgNeuronGroup()->getNumNeurons() * batchSize,
                        sg->getInSynLocation());
            
            if (sg->isDendriticDelayRequired()) {
                createArray(sg, "denDelay", getModel().getPrecision(), 
                            (size_t)sg->getMaxDendriticDelayTimesteps() * (size_t)sg->getTrgNeuronGroup()->getNumNeurons() * batchSize,
                            sg->getDendriticDelayLocation());
                createArray(sg, "denDelayPtr", Type::Uint32, 1, VarLocation::DEVICE);
            }

            // Create arrays for postsynaptic model state variables
            createNeuronVarArrays<SynapsePSMVarAdapter>(sg, sg->getTrgNeuronGroup()->getNumNeurons(), batchSize, 1, true);
        }

        // Create arrays for fused pre-output variables
        for(const auto *sg : n.second.getFusedPreOutputOutSyn()) {
            createArray(sg, "outPre", getModel().getPrecision(), 
                        sg->getSrcNeuronGroup()->getNumNeurons() * batchSize,
                        sg->getInSynLocation());
        }
        
        // Create arrays for variables from fused incoming synaptic populations
        for(const auto *sg: n.second.getFusedWUPreOutSyn()) {
            const unsigned int preDelaySlots = (sg->getDelaySteps() == NO_DELAY) ? 1 : sg->getSrcNeuronGroup()->getNumDelaySlots();
            createNeuronVarArrays<SynapseWUPreVarAdapter>(sg, sg->getSrcNeuronGroup()->getNumNeurons(), batchSize, preDelaySlots, true);
        }
        
        // Create arrays for variables from fused outgoing synaptic populations
        for(const auto *sg: n.second.getFusedWUPostInSyn()) { 
            const unsigned int postDelaySlots = (sg->getBackPropDelaySteps() == NO_DELAY) ? 1 : sg->getTrgNeuronGroup()->getNumDelaySlots();
            createNeuronVarArrays<SynapseWUPostVarAdapter>(sg, sg->getTrgNeuronGroup()->getNumNeurons(), batchSize, postDelaySlots, true);
        }

        // Create arrays for spikes
        for(const auto *sg: n.second.getFusedSpike()) {
            createArray(sg, "spkCnt", Type::Uint32, batchSize * n.second.getNumDelaySlots(), 
                        n.second.getSpikeLocation());
            createArray(sg, "spk", Type::Uint32, numNeuronDelaySlots, 
                        n.second.getSpikeLocation());

            // If neuron group needs to record its spike times
            if (n.second.isSpikeTimeRequired()) {
                createArray(sg, "sT", getModel().getTimePrecision(), numNeuronDelaySlots, 
                            n.second.getSpikeTimeLocation());
            }

            // If neuron group needs to record its previous spike times
            if (n.second.isPrevSpikeTimeRequired()) {
                createArray(sg, "prevST", getModel().getTimePrecision(), numNeuronDelaySlots, 
                            n.second.getPrevSpikeTimeLocation());
            }
        }

        // Create arrays for spike events
        for(const auto *sg: n.second.getFusedSpikeEvent()) {
            createArray(sg, "spkCntEvent", Type::Uint32, batchSize * n.second.getNumDelaySlots(), 
                        n.second.getSpikeEventLocation());
            createArray(sg, "spkEvent", Type::Uint32, numNeuronDelaySlots, 
                        n.second.getSpikeEventLocation());

            // If neuron group needs to record its spike-like-event times
            if (n.second.isSpikeEventTimeRequired()) {
                createArray(sg, "seT", getModel().getTimePrecision(), numNeuronDelaySlots, 
                            n.second.getSpikeEventTimeLocation());
            }

            // If neuron group needs to record its previous spike-like-event times
            if (n.second.isPrevSpikeEventTimeRequired()) {
                createArray(sg, "prevSET", getModel().getTimePrecision(), numNeuronDelaySlots, 
                            n.second.getPrevSpikeEventTimeLocation());
            }

            if(n.second.isSpikeEventRecordingEnabled()) {
                createArray(sg, "recordSpkEvent", Type::Uint32, numRecordingWords, 
                            VarLocation::HOST_DEVICE);
            }
        }
    }

    // Loop through synapse groups
    for(const auto &s : getModel().getSynapseGroups()) {
        // If synapse group has individual or kernel weights
        LOGD_RUNTIME << "Allocating memory for synapse group '" << s.first << "'";
        const bool individualWeights = (s.second.getMatrixType() & SynapseMatrixWeight::INDIVIDUAL);
        const bool kernelWeights = (s.second.getMatrixType() & SynapseMatrixWeight::KERNEL);
        if (individualWeights || kernelWeights) {
            createVarArrays<SynapseWUVarAdapter>(
                &s.second, batchSize, true, 
                [&s, this](const std::string&, VarAccessDim varDims)
                {
                    return getNumSynapseVarElements(varDims, m_Backend.get(), s.second);
                });
        }

        // Create destinations for any dynamic parameters
        createDynamicParamDestinations<SynapseGroupInternal>(s.second, s.second.getWUInitialiser().getSnippet()->getParams(),
                                                            &SynapseGroupInternal::isWUParamDynamic);
        createDynamicParamDestinations<SynapseGroupInternal>(s.second, s.second.getPSInitialiser().getSnippet()->getParams(),
                                                            &SynapseGroupInternal::isPSParamDynamic);

        // If connectivity is bitmask
        const size_t numPre = s.second.getSrcNeuronGroup()->getNumNeurons();
        const size_t rowStride = m_Backend.get().getSynapticMatrixRowStride(s.second);
        const auto &connectInit = s.second.getConnectivityInitialiser();
        const bool uninitialized = (Utils::areTokensEmpty(connectInit.getRowBuildCodeTokens()) 
                                    && Utils::areTokensEmpty(connectInit.getColBuildCodeTokens()));

        if(s.second.getMatrixType() & SynapseMatrixConnectivity::BITMASK) {
            const size_t gpSize = ceilDivide((size_t)numPre * rowStride, 32);
            createArray(&s.second, "gp", Type::Uint32, gpSize,
                        s.second.getSparseConnectivityLocation(), uninitialized);
            
            // If this isn't uninitialised i.e. it will be 
            // initialised using initialization kernel, zero bitmask
            if(!uninitialized) {
                if(m_Backend.get().isArrayDeviceObjectRequired()) {
                    getArray(s.second, "gp")->memsetDeviceObject(0);
                }
                else {
                    getArray(s.second, "gp")->memsetHostPointer(0);
                }
            }
        }
        // Otherwise, if connectivity is sparse
        else if(s.second.getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
            // Row lengths
            createArray(&s.second, "rowLength", Type::Uint32, numPre,
                        s.second.getSparseConnectivityLocation(), uninitialized);

            // Target indices
            createArray(&s.second, "ind", s.second.getSparseIndType(), numPre * rowStride,
                        s.second.getSparseConnectivityLocation(), uninitialized);
            
            // If this isn't uninitialised i.e. it will be 
            // initialised using initialization kernel, zero row length
            if(!uninitialized) {
                LOGD_RUNTIME << "\tZeroing 'rowLength'";
                if(m_Backend.get().isArrayDeviceObjectRequired()) {
                    getArray(s.second, "rowLength")->memsetDeviceObject(0);
                }
                else {
                    getArray(s.second, "rowLength")->memsetHostPointer(0);
                }
            }

            // **TODO** remap is not always required
            if(m_Backend.get().isPostsynapticRemapRequired() && !Utils::areTokensEmpty(s.second.getWUInitialiser().getPostLearnCodeTokens())) {
                // Create column lengths array
                const size_t numPost = s.second.getTrgNeuronGroup()->getNumNeurons();
                const size_t colStride = s.second.getMaxSourceConnections();
                createArray(&s.second, "colLength", Type::Uint32, numPost, VarLocation::DEVICE);
                
                // Create remap array
                createArray(&s.second, "remap", Type::Uint32, numPost * colStride, VarLocation::DEVICE);

                // Zero column length array
                LOGD_RUNTIME << "\tZeroing 'colLength'";
                if(m_Backend.get().isArrayDeviceObjectRequired()) {
                    getArray(s.second, "colLength")->memsetDeviceObject(0);
                }
                else {
                    getArray(s.second, "colLength")->memsetHostPointer(0);
                }
            }
        }

        // Loop through sparse connectivity initialiser EGPs
        // **THINK** should any of these have locations? if they're not initialised in host code not much scope to do so
        const auto &sparseConnectInit = s.second.getConnectivityInitialiser();
        for(const auto &egp : sparseConnectInit.getSnippet()->getExtraGlobalParams()) {
            const auto resolvedEGPType = egp.type.resolve(getModel().getTypeContext());
            createArray(&s.second, egp.name + "SparseConnect", resolvedEGPType, 0, VarLocation::HOST_DEVICE);
        }

        // Loop through toeplitz connectivity initialiser EGPs        
        const auto &toeplitzConnectInit = s.second.getToeplitzConnectivityInitialiser();
        for(const auto &egp : toeplitzConnectInit.getSnippet()->getExtraGlobalParams()) {
            const auto resolvedEGPType = egp.type.resolve(getModel().getTypeContext());
            createArray(&s.second, egp.name + "ToeplitzConnect", resolvedEGPType, 0, VarLocation::HOST_DEVICE);
        }

        // Create arrays for extra-global parameters
        // **NOTE** postsynaptic models with EGPs can't be fused so no need to worry about that
        createEGPArrays<SynapseWUEGPAdapter>(&s.second);
        createEGPArrays<SynapsePSMEGPAdapter>(&s.second);
    }

    // Allocate custom update variables
    for(const auto &c : getModel().getCustomUpdates()) {
        LOGD_RUNTIME << "Allocating memory for custom update '" << c.first << "'";
        createNeuronVarArrays<CustomUpdateVarAdapter>(&c.second, c.second.getSize(), batchSize, 1, 
                                                      c.second.getDims() & VarAccessDim::BATCH);
        // Create arrays for custom update extra global parameters
        createEGPArrays<CustomUpdateEGPAdapter>(&c.second);

        createDynamicParamDestinations<CustomUpdateInternal>(c.second, c.second.getCustomUpdateModel()->getParams(),
                                                            &CustomUpdateInternal::isParamDynamic);
    }

    // Allocate custom update WU variables
    for(const auto &c : getModel().getCustomWUUpdates()) {
        LOGD_RUNTIME << "Allocating memory for custom WU update '" << c.first << "'";
        createVarArrays<CustomUpdateVarAdapter>(
                &c.second, batchSize, (c.second.getDims() & VarAccessDim::BATCH), 
                [&c, this](const std::string&, VarAccessDim varDims)
                {
                    return getNumSynapseVarElements(varDims, m_Backend.get(), 
                                                    *c.second.getSynapseGroup());
                });
        
        // Create arrays for custom update extra global parameters
        createEGPArrays<CustomUpdateEGPAdapter>(&c.second);

        createDynamicParamDestinations<CustomUpdateWUInternal>(c.second, c.second.getCustomUpdateModel()->getParams(),
                                                               &CustomUpdateWUInternal::isParamDynamic);
    }

    // Loop through custom connectivity update variables
    for(const auto &c : getModel().getCustomConnectivityUpdates()) {
        // Allocate presynaptic variables
        LOGD_RUNTIME << "Allocating memory for custom connectivity update '" << c.first << "'";
        createNeuronVarArrays<CustomConnectivityUpdatePreVarAdapter>(
            &c.second, c.second.getSynapseGroup()->getSrcNeuronGroup()->getNumNeurons(),
            batchSize, 1, false);
        
        // Allocate postsynaptic variables
        createNeuronVarArrays<CustomConnectivityUpdatePostVarAdapter>(
            &c.second, c.second.getSynapseGroup()->getTrgNeuronGroup()->getNumNeurons(),
            batchSize, 1, false);

        // Allocate variables
        createVarArrays<CustomConnectivityUpdateVarAdapter>(
                &c.second, batchSize, false, 
                [&c, this](const std::string&, VarAccessDim varDims)
                {
                    return getNumSynapseVarElements(varDims, m_Backend.get(), 
                                                    *c.second.getSynapseGroup());
                });
        
        // Create arrays for custom connectivity update extra global parameters
        createEGPArrays<CustomConnectivityUpdateEGPAdapter>(&c.second);

        createDynamicParamDestinations<CustomConnectivityUpdateInternal>(
            c.second, c.second.getCustomConnectivityUpdateModel()->getParams(),
            &CustomConnectivityUpdateInternal::isParamDynamic);

        // If custom connectivity update group needs per-row RNGs
        if(Utils::isRNGRequired(c.second.getRowUpdateCodeTokens())) {
            auto rng = m_Backend.get().createPopulationRNG(
                c.second.getSynapseGroup()->getSrcNeuronGroup()->getNumNeurons());
            if(rng) {
                const auto r = m_CustomConnectivityUpdateArrays[&c.second].try_emplace("rowRNG", std::move(rng));
                if(!r.second) {
                    throw std::runtime_error("Unable to allocate array with " 
                                             "duplicate name 'rowRNG'");
                }
            }
        }
    }
    
    // Push merged synapse host connectivity initialisation groups 
    for(const auto &m : m_ModelMerged.get().getMergedSynapseConnectivityHostInitGroups()) {
       pushMergedGroup(m);
    }

    // Perform host initialisation
    m_InitializeHost();

    // Push merged neuron initialisation groups
    for(const auto &m : m_ModelMerged.get().getMergedNeuronInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged synapse init groups
    for(const auto &m : m_ModelMerged.get().getMergedSynapseInitGroups()) {
         addMergedArrays(m);
         pushMergedGroup(m);
    }

    // Push merged synapse connectivity initialisation groups
    for(const auto &m : m_ModelMerged.get().getMergedSynapseConnectivityInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged sparse synapse init groups
    for(const auto &m : m_ModelMerged.get().getMergedSynapseSparseInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged custom update initialisation groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomUpdateInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged custom WU update initialisation groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomWUUpdateInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged custom sparse WU update initialisation groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomWUUpdateSparseInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged custom connectivity update presynaptic initialisation groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomConnectivityUpdatePreInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged custom connectivity update postsynaptic initialisation groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomConnectivityUpdatePostInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged custom connectivity update synaptic initialisation groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomConnectivityUpdateSparseInitGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged neuron update groups
    for(const auto &m : m_ModelMerged.get().getMergedNeuronUpdateGroups()) {        
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged presynaptic update groups
    for(const auto &m : m_ModelMerged.get().getMergedPresynapticUpdateGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push merged postsynaptic update groups
    for(const auto &m : m_ModelMerged.get().getMergedPostsynapticUpdateGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push synapse dynamics groups
    for(const auto &m : m_ModelMerged.get().getMergedSynapseDynamicsGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push neuron groups whose previous spike times need resetting
    for(const auto &m : m_ModelMerged.get().getMergedNeuronPrevSpikeTimeUpdateGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push neuron groups whose spike queues need resetting
    for(const auto &m : m_ModelMerged.get().getMergedNeuronSpikeQueueUpdateGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push synapse groups whose dendritic delay pointers need updating
    for(const auto &m : m_ModelMerged.get().getMergedSynapseDendriticDelayUpdateGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }
    
    // Push custom variable update groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomUpdateGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push custom WU variable update groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomUpdateWUGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push custom WU transpose variable update groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomUpdateTransposeWUGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push custom update host reduction groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomUpdateHostReductionGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push custom weight update host reduction groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomWUUpdateHostReductionGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push custom connectivity update groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomConnectivityUpdateGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }

    // Push custom connectivity host update groups
    for(const auto &m : m_ModelMerged.get().getMergedCustomConnectivityHostUpdateGroups()) {
        addMergedArrays(m);
        pushMergedGroup(m);
    }
}
//----------------------------------------------------------------------------
void Runtime::initialize()
{
    m_Initialize();
}
//----------------------------------------------------------------------------
void Runtime::initializeSparse()
{
    // Push uninitialized arrays to device
    LOGD_RUNTIME << "Pushing uninitialized current source variables";
    pushUninitialized(m_CurrentSourceArrays);
    LOGD_RUNTIME << "Pushing uninitialized neuron group variables";
    pushUninitialized(m_NeuronGroupArrays);
    LOGD_RUNTIME << "Pushing uninitialized synapse group variables";
    pushUninitialized(m_SynapseGroupArrays);
    LOGD_RUNTIME << "Pushing uninitialized custom update variables";
    pushUninitialized(m_CustomUpdateBaseArrays);
    LOGD_RUNTIME << "Pushing uninitialized custom connectivity update variables";
    pushUninitialized(m_CustomConnectivityUpdateArrays);

    m_InitializeSparse();
}
//----------------------------------------------------------------------------
void Runtime::stepTime()
{
   m_StepTime(m_Timestep, m_NumRecordingTimesteps.value_or(0));
    
   // Loop through delay queue pointers and update
   for(auto &d : m_DelayQueuePointer) {
       d.second = (d.second + 1) % d.first->getNumDelaySlots();
   }

    // Advance time
    m_Timestep++;
}
//----------------------------------------------------------------------------
double Runtime::getTime() const
{ 
    return m_Timestep * getModel().getDT();
}
//----------------------------------------------------------------------------
void Runtime::pullRecordingBuffersFromDevice() const
{
    if(!m_NumRecordingTimesteps) {
        throw std::runtime_error("Recording buffer not allocated - cannot pull from device");
    }

    // Loop through neuron groups
    for(const auto &n : getModel().getNeuronGroups()) {
        // If spike recording is enabled, pull array from device
        if(n.second.isSpikeRecordingEnabled()) {
            getArray(n.second, "recordSpk")->pullFromDevice();
        }

        // If spike event recording is enabled, pull array from device
        if(n.second.isSpikeEventRecordingEnabled()) {
            for(const auto *sg : n.second.getFusedSpikeEvent()) {
                getArray(*sg, "recordSpkEvent")->pullFromDevice();
            }
        }
    }
}
//----------------------------------------------------------------------------
const ModelSpecInternal &Runtime::getModel() const
{
    return m_ModelMerged.get().getModel();
}
//----------------------------------------------------------------------------
void *Runtime::getSymbol(const std::string &symbolName, bool allowMissing) const
{
#ifdef _WIN32
    void *symbol = GetProcAddress(m_Library, symbolName.c_str());
#else
    void *symbol = dlsym(m_Library, symbolName.c_str());
#endif

    // Return symbol if it's found
    if(symbol) {
        return symbol;
    }
    // Otherwise
    else {
        // If this isn't allowed, throw error
        if(!allowMissing) {
            throw std::runtime_error("Cannot find symbol '" + symbolName + "'");
        }
        // Otherwise, return default
        else {
            return nullptr;
        }
    }
}
//----------------------------------------------------------------------------
void Runtime::createArray(ArrayMap &groupArrays, const std::string &varName, const Type::ResolvedType &type, 
                          size_t count, VarLocation location, bool uninitialized)
{
    LOGD_RUNTIME << "\tArray '" << varName << "' = " << count << " * " << type.getSize(m_Backend.get().getPointerBytes()) << " bytes (" << type.getName() << ")";
    const auto r = groupArrays.try_emplace(varName, m_Backend.get().createArray(type, count, location, uninitialized));
    if(!r.second) {
        throw std::runtime_error("Unable to allocate array with " 
                                 "duplicate name '" + varName + "'");
    }
}
//----------------------------------------------------------------------------
void Runtime::createDynamicParamDestinations(std::unordered_map<std::string, std::pair<Type::ResolvedType, MergedDynamicFieldDestinations>> &destinations, 
                                             const std::string &paramName, const Type::ResolvedType &type)
{
    LOGD_RUNTIME << "\tDynamic param '" << paramName << "' (" << type.getName() << ")";
    const auto r = destinations.try_emplace(paramName, std::make_pair(type, MergedDynamicFieldDestinations()));
    if(!r.second) {
        throw std::runtime_error("Unable to add dynamic parameter with " 
                                 "duplicate name '" + paramName + "'");
    }
}
//----------------------------------------------------------------------------
Runtime::BatchEventArray Runtime::getRecordedEvents(unsigned int numNeurons, ArrayBase *array) const
{
    if(!m_NumRecordingTimesteps) {
        throw std::runtime_error("Recording buffer not allocated - cannot get recorded events");
    }

    // Calculate number of words per-timestep
    const size_t timestepWords = ceilDivide(numNeurons, 32);

    if(m_Timestep < *m_NumRecordingTimesteps) {
        throw std::runtime_error("Event recording data can only be accessed once buffer is full");
    }
    
    // Calculate start time
    const double dt = getModel().getDT();
    const double startTime = (m_Timestep - *m_NumRecordingTimesteps) * dt;

    // Loop through timesteps
    const uint32_t *spkRecordWords = reinterpret_cast<const uint32_t*>(array->getHostPointer());
    BatchEventArray events(getModel().getBatchSize());
    for(size_t t = 0; t < m_NumRecordingTimesteps.value(); t++) {
        // Loop through batched
        const double time = startTime + (t * dt);
        for(size_t b = 0; b < getModel().getBatchSize(); b++) {
            // Loop through words representing timestep
            auto &batchEvents = events[b];
            for(size_t w = 0; w < timestepWords; w++) {
                // Get word
                uint32_t spikeWord = *spkRecordWords++;
            
                // Calculate neuron id of highest bit of this word
                unsigned int neuronID = (w * 32) + 31;
            
                // While bits remain
                while(spikeWord != 0) {
                    // Calculate leading zeros
                    const int numLZ = Utils::clz(spikeWord);
                
                    // If all bits have now been processed, zero spike word
                    // Otherwise shift past the spike we have found
                    spikeWord = (numLZ == 31) ? 0 : (spikeWord << (numLZ + 1));
                
                    // Subtract number of leading zeros from neuron ID
                    neuronID -= numLZ;
                
                    // Add time and ID to vectors
                    batchEvents.first.push_back(time);
                    batchEvents.second.push_back(neuronID);
                
                    // New neuron id of the highest bit of this word
                    neuronID--;
                }
            }
        }
    }

    // Return vectors
    return events;
}
//----------------------------------------------------------------------------
void Runtime::writeRecordedEvents(unsigned int numNeurons, ArrayBase *array, const std::string &path) const
{
    // Get events
    const auto events = getRecordedEvents(numNeurons, array);

    // Open file and write header
    std::ofstream file(path);
    file << "Time [ms], Neuron ID";
    if(getModel().getBatchSize() > 1) {
        file << ", Batch";
    }
    file << std::endl;

    // Loop through batches;
    for(size_t b = 0; b < getModel().getBatchSize(); b++) {
        // Loop through events
        const auto &batchEvents = events[b];
        auto t = batchEvents.first.cbegin();
        auto i = batchEvents.second.cbegin();
        for(;t < batchEvents.first.cend(); t++, i++) {
            // Write to file
            file << *t << ", " << *i;
            if(getModel().getBatchSize() > 1) {
                file << ", " << b;
            }
            file << std::endl;
        }
    }
}
//----------------------------------------------------------------------------
void Runtime::setDynamicParamValue(const std::pair<Type::ResolvedType, MergedDynamicFieldDestinations> &mergedDestinations, 
                                   const Type::NumericValue &value)
{
    // Serailise new value
    std::vector<std::byte> valueStorage;
    Type::serialiseNumeric(value, mergedDestinations.first, valueStorage);

    // Build FFI arguments
    ffi_type *argumentTypes[2]{&ffi_type_uint, mergedDestinations.first.getFFIType()};

    // Prepare an FFI Call InterFace for calls to push merged
    // **TODO** cache - these are the same for all calls with same datatype
    ffi_cif cif;
    ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2,
                                     &ffi_type_void, argumentTypes);
    if (status != FFI_OK) {
        throw std::runtime_error("ffi_prep_cif failed: " + std::to_string(status));
    }
    
    // Loop through merged destinations of this array
    for(const auto &d : mergedDestinations.second.getDestinationFields()) {
        // Get push function
        // **TODO** cache in structure instead of mergedGroup and fieldName
        void *pushFunction = getSymbol("pushMerged" + d.first + std::to_string(d.second.mergedGroupIndex) 
                                       + d.second.fieldName + "ToDevice");

        // Call function
        unsigned int groupIndex = d.second.groupIndex;
        void *argumentPointers[2]{&groupIndex, valueStorage.data()};
        ffi_call(&cif, FFI_FN(pushFunction), nullptr, argumentPointers);
    }
}
//----------------------------------------------------------------------------
void Runtime::allocateExtraGlobalParam(ArrayMap &groupArrays, const std::string &varName,
                                       size_t count)
{
    // Find array
    auto *array = groupArrays.at(varName).get();

    // Allocate array
    array->allocate(count);

    // Serialise host pointer
    std::vector<std::byte> serialisedHostPointer;
    array->serialiseHostPointer(serialisedHostPointer, false);

    // If backend requires it, serialise device object
    std::vector<std::byte> serialisedDeviceObject;
    if(m_Backend.get().isArrayDeviceObjectRequired()) {
        array->serialiseDeviceObject(serialisedDeviceObject, false);
    }
    
    // If backend requires it, serialise host object
    std::vector<std::byte> serialisedHostObject;
    if(m_Backend.get().isArrayHostObjectRequired()) {
        array->serialiseHostObject(serialisedHostObject, false);
    }

    // Build FFI arguments
    // **TODO** allow backend to override type
    ffi_type *argumentTypes[2]{&ffi_type_uint, &ffi_type_pointer};

    // Prepare an FFI Call InterFace for calls to push merged
    // **TODO** cache - these are the same for all EGP calls
    ffi_cif cif;
    ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2,
                                     &ffi_type_void, argumentTypes);
    if (status != FFI_OK) {
        throw std::runtime_error("ffi_prep_cif failed: " + std::to_string(status));
    }
    
    // Loop through merged destinations of this array
    const auto &mergedDestinations = m_MergedDynamicArrays.at(array);
    for(const auto &d : mergedDestinations.getDestinationFields()) {
        // Get push function
        // **TODO** cache in structure instead of mergedGroup and fieldName
        void *pushFunction = getSymbol("pushMerged" + d.first + std::to_string(d.second.mergedGroupIndex) 
                                       + d.second.fieldName + "ToDevice");

        // Call function
        unsigned int groupIndex = d.second.groupIndex;
        void *argumentPointers[2]{&groupIndex, nullptr};
        if(d.second.fieldType & GroupMergedFieldType::HOST) {
            assert(!serialisedHostPointer.empty());
            argumentPointers[1] = serialisedHostPointer.data();
        }
        else if(d.second.fieldType & GroupMergedFieldType::HOST_OBJECT) {
            assert(!serialisedHostObject.empty());
            argumentPointers[1] = serialisedHostObject.data();
        }
        // Serialise device object if backend requires it
        else {
            if(m_Backend.get().isArrayDeviceObjectRequired()) {
                assert(!serialisedDeviceObject.empty());
                argumentPointers[1] = serialisedDeviceObject.data();
            }
            // Otherwise, host pointer
            else {
                assert(!serialisedHostPointer.empty());
                argumentPointers[1] = serialisedHostPointer.data();
            }
        }
        ffi_call(&cif, FFI_FN(pushFunction), nullptr, argumentPointers);
    }
}
}   // namespace GeNN::Runtime