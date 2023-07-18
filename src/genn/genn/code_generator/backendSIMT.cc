#include "code_generator/backendSIMT.h"

// Standard C++ includes
#include <algorithm>

// GeNN code generator includes
#include "code_generator/modelSpecMerged.h"

//-----------------------------------------------------------------------
// Anonymous namespace
//-----------------------------------------------------------------------
namespace
{
template<typename T, typename G>
size_t getNumMergedGroupThreads(const std::vector<T> &groups, G getNumThreads)
{
    // Accumulate the accumulation of all groups in merged group
    return std::accumulate(
        groups.cbegin(), groups.cend(), size_t{0},
        [getNumThreads](size_t acc, const T &n)
        {
            return std::accumulate(n.getGroups().cbegin(), n.getGroups().cend(), acc,
                                   [getNumThreads](size_t acc, std::reference_wrapper<const typename T::GroupInternal> g)
            {
                return acc + getNumThreads(g.get());
            });
        });
}
}   // Anonymous namespace

//--------------------------------------------------------------------------
// GeNN::CodeGenerator::BackendSIMT
//--------------------------------------------------------------------------
namespace GeNN::CodeGenerator
{
const char *BackendSIMT::KernelNames[KernelMax] = {
    "updateNeuronsKernel",
    "updatePresynapticKernel",
    "updatePostsynapticKernel",
    "updateSynapseDynamicsKernel",
    "initializeKernel",
    "initializeSparseKernel",
    "neuronSpikeQueueUpdateKernel",
    "neuronPrevSpikeTimeUpdateKernel",
    "synapseDendriticDelayUpdateKernel",
    "customUpdate",
    "customTransposeUpdate"};
//--------------------------------------------------------------------------
std::vector<PresynapticUpdateStrategySIMT::Base*> BackendSIMT::s_PresynapticUpdateStrategies = {
    new PresynapticUpdateStrategySIMT::PreSpan,
    new PresynapticUpdateStrategySIMT::PostSpan,
    new PresynapticUpdateStrategySIMT::PreSpanProcedural,
    new PresynapticUpdateStrategySIMT::PostSpanBitmask,
    new PresynapticUpdateStrategySIMT::PostSpanToeplitz};
//--------------------------------------------------------------------------
size_t BackendSIMT::getSynapticMatrixRowStride(const SynapseGroupInternal &sg) const
{
    return getPresynapticUpdateStrategy(sg)->getSynapticMatrixRowStride(sg);
}
//--------------------------------------------------------------------------
void BackendSIMT::genPopVariableInit(EnvironmentExternalBase &env, HandlerEnv handler) const
{
    // If this is first thread in group
    env.getStream() << "if(" << env["id"] << " == 0)";
    {
        CodeStream::Scope b(env.getStream());
        handler(env);
    }
}
//--------------------------------------------------------------------------
void BackendSIMT::genVariableInit(EnvironmentExternalBase &env, const std::string&, const std::string&, HandlerEnv handler) const
{
    // Variable should already be provided via parallelism
    //assert(kernelSubs.hasVarSubstitution(countVarName));

    handler(env);
}
//--------------------------------------------------------------------------
void BackendSIMT::genKernelSynapseVariableInit(EnvironmentExternalBase &env, SynapseInitGroupMerged&, HandlerEnv handler) const
{
    // Variable should already be provided via parallelism
    //assert(kernelSubs.hasVarSubstitution("id"));
    
    EnvironmentExternal varEnv(env);
    varEnv.add(Type::Uint32.addConst(), "id_syn", "$(id)");

    handler(varEnv);
}
//--------------------------------------------------------------------------
void BackendSIMT::genKernelCustomUpdateVariableInit(EnvironmentExternalBase &env, CustomWUUpdateInitGroupMerged &, HandlerEnv handler) const
{
    // Variable should already be provided via parallelism
    //assert(kernelSubs.hasVarSubstitution("id"));

    EnvironmentExternal varEnv(env);
    varEnv.add(Type::Uint32.addConst(), "id_syn", "$(id)");

    handler(varEnv);
}
//--------------------------------------------------------------------------
bool BackendSIMT::isGlobalHostRNGRequired(const ModelSpecInternal &model) const
{
    // Host RNG is required if any synapse groups or custom connectivity updates require a host RNG
    return (std::any_of(model.getSynapseGroups().cbegin(), model.getSynapseGroups().cend(),
                        [](const ModelSpec::SynapseGroupValueType &s){ return s.second.getConnectivityInitialiser().isHostRNGRequired(); })
            || std::any_of(model.getCustomConnectivityUpdates().cbegin(), model.getCustomConnectivityUpdates().cend(),
                           [](const ModelSpec::CustomConnectivityUpdateValueType &c){ return Utils::isRNGRequired(c.second.getHostUpdateCodeTokens()); }));
}
//--------------------------------------------------------------------------
bool BackendSIMT::isGlobalDeviceRNGRequired(const ModelSpecInternal &model) const
{
    // If any neuron groups require  RNG for initialisation, return true
    // **NOTE** this takes postsynaptic model initialisation into account
    if(std::any_of(model.getNeuronGroups().cbegin(), model.getNeuronGroups().cend(),
                   [](const ModelSpec::NeuronGroupValueType &n){ return n.second.isInitRNGRequired(); }))
    {
        return true;
    }

    // If any synapse groups require an RNG for weight update model initialisation or procedural connectivity, return true
    if(std::any_of(model.getSynapseGroups().cbegin(), model.getSynapseGroups().cend(),
                   [](const ModelSpec::SynapseGroupValueType &s)
                   {
                       return (s.second.isWUInitRNGRequired() || s.second.isProceduralConnectivityRNGRequired());
                   }))
    {
        return true;
    }

    // If any custom updates require an RNG fo initialisation, return true
    if(std::any_of(model.getCustomUpdates().cbegin(), model.getCustomUpdates().cend(),
                   [](const ModelSpec::CustomUpdateValueType &c)
                   {
                       return (c.second.isInitRNGRequired());
                   }))
    {
        return true;
    }

    // If any custom WU updates require an RNG fo initialisation, return true
    if(std::any_of(model.getCustomWUUpdates().cbegin(), model.getCustomWUUpdates().cend(),
                   [](const ModelSpec::CustomUpdateWUValueType &c)
                   {
                       return (c.second.isInitRNGRequired());
                   }))
    {
        return true;
    }

    return false;
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getNumInitialisationRNGStreams(const ModelSpecMerged &modelMerged) const
{
    // Calculate total number of threads used for neuron initialisation group
    size_t numInitThreads = getNumMergedGroupThreads(modelMerged.getMergedNeuronInitGroups(),
                                                     [this](const NeuronGroupInternal &ng)
                                                     {
                                                         return padKernelSize(ng.getNumNeurons(), KernelInitialize);
                                                     });

    // Add on total number of threads used for custom update initialisation
    numInitThreads += getNumMergedGroupThreads(modelMerged.getMergedCustomUpdateInitGroups(),
                                               [this](const CustomUpdateInternal &cg)
                                               {
                                                   return padKernelSize(cg.getSize(), KernelInitialize);
                                               });
    
    // Add on total number of threads used for custom WU update initialization
    numInitThreads += getNumMergedGroupThreads(modelMerged.getMergedCustomWUUpdateInitGroups(),
                                               [this](const CustomUpdateWUInternal &cg)
                                               {
                                                   return padKernelSize(getNumInitThreads(cg), KernelInitialize);
                                               });
    
    // Add on total number of threads used for synapse initialisation
    numInitThreads += getNumMergedGroupThreads(modelMerged.getMergedSynapseInitGroups(),
                                               [this](const SynapseGroupInternal &sg)
                                               {
                                                   return padKernelSize(getNumInitThreads(sg), KernelInitialize);
                                               });

    // Add on total number of threads used for synapse connectivity initialisation
    numInitThreads += getNumMergedGroupThreads(modelMerged.getMergedSynapseConnectivityInitGroups(),
                                               [this](const SynapseGroupInternal &sg)
                                               {
                                                   return padKernelSize(getNumConnectivityInitThreads(sg), KernelInitialize);
                                               });

    // Add on total number of threads used for sparse synapse initialisation
    numInitThreads += getNumMergedGroupThreads(modelMerged.getMergedSynapseSparseInitGroups(),
                                               [this](const SynapseGroupInternal &sg)
                                               {
                                                   return padKernelSize(sg.getMaxConnections(), KernelInitializeSparse);
                                               });
    
    // Finally, add on total number of threads used for custom WU update groups with sparse connectivity
    numInitThreads += getNumMergedGroupThreads(modelMerged.getMergedCustomWUUpdateSparseInitGroups(),
                                               [this](const CustomUpdateWUInternal &cg)
                                               {
                                                   return padKernelSize(cg.getSynapseGroup()->getMaxConnections(), KernelInitializeSparse);
                                               });
    return numInitThreads;
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getPaddedNumCustomUpdateThreads(const CustomUpdateInternal &cg, unsigned int batchSize) const
{
    const size_t numCopies = (cg.isBatched() && !cg.isBatchReduction()) ? batchSize : 1;

    if (cg.isNeuronReduction()) {
        return padKernelSize(32 * numCopies, KernelCustomUpdate);
    }
    else if (cg.isPerNeuron()) {
        return numCopies * padKernelSize(cg.getSize(), KernelCustomUpdate);
    }
    else {
        return padKernelSize(numCopies, KernelCustomUpdate);
    }
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getPaddedNumCustomUpdateWUThreads(const CustomUpdateWUInternal &cg, unsigned int batchSize) const
{
    const SynapseGroupInternal *sgInternal = static_cast<const SynapseGroupInternal*>(cg.getSynapseGroup());
    const size_t numCopies = (cg.isBatched() && !cg.isBatchReduction()) ? batchSize : 1;

    if(sgInternal->getMatrixType() & SynapseMatrixWeight::KERNEL) {
        return numCopies * padKernelSize(sgInternal->getKernelSizeFlattened(), KernelCustomUpdate);
    }
    else {
        return numCopies * padKernelSize((size_t)sgInternal->getSrcNeuronGroup()->getNumNeurons() * sgInternal->getMaxConnections(),
                                         KernelCustomUpdate);
    }
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getPaddedNumCustomUpdateTransposeWUThreads(const CustomUpdateWUInternal &cg, unsigned int batchSize) const
{
    assert(cg.isTransposeOperation());
    assert(cg.getSynapseGroup()->getMatrixType() & SynapseMatrixConnectivity::DENSE);
    
    const size_t paddedNumPre = padKernelSize(cg.getSynapseGroup()->getSrcNeuronGroup()->getNumNeurons(), KernelCustomTransposeUpdate);
    const size_t paddedNumPost = padKernelSize(cg.getSynapseGroup()->getTrgNeuronGroup()->getNumNeurons(), KernelCustomTransposeUpdate);
    const size_t numCopies = cg.isBatched() ? batchSize : 1;
    return numCopies * paddedNumPre * paddedNumPost / getKernelBlockSize(KernelCustomTransposeUpdate);
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getNumPresynapticUpdateThreads(const SynapseGroupInternal &sg, const PreferencesBase &preferences)
{
    return getPresynapticUpdateStrategy(sg, preferences)->getNumThreads(sg);
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getNumPostsynapticUpdateThreads(const SynapseGroupInternal &sg)
{
    if(sg.getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
        return sg.getMaxSourceConnections();
    }
    else {
        return sg.getSrcNeuronGroup()->getNumNeurons();
    }
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getNumSynapseDynamicsThreads(const SynapseGroupInternal &sg)
{
    if(sg.getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
        // **THINK** this isn't really correct but correct value is inaccesible
        return (size_t)sg.getSrcNeuronGroup()->getNumNeurons() * sg.getMaxConnections();
    }
    else {
        return (size_t)sg.getSrcNeuronGroup()->getNumNeurons() * sg.getTrgNeuronGroup()->getNumNeurons();
    }
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getNumConnectivityInitThreads(const SynapseGroupInternal &sg)
{
    // If there's row building code, return number of source neurons i.e. rows
    if(!sg.getConnectivityInitialiser().getSnippet()->getRowBuildCode().empty()) {
        return sg.getSrcNeuronGroup()->getNumNeurons();
    }
    // Otherwise, if there's column building code, return number of target neurons i.e. columns
    else if(!sg.getConnectivityInitialiser().getSnippet()->getColBuildCode().empty()) {
        return sg.getTrgNeuronGroup()->getNumNeurons();
    }
    // Otherwise, give an error
    else {
        throw std::runtime_error("Cannot calculate number of connectivity init threads without connectivity building code");
    }
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getNumInitThreads(const SynapseGroupInternal &sg)
{
    if (sg.getMatrixType() & SynapseMatrixWeight::KERNEL) {
        return sg.getKernelSizeFlattened();
    }
    else {
        return sg.getTrgNeuronGroup()->getNumNeurons();
    }
}
//--------------------------------------------------------------------------
size_t BackendSIMT::getNumInitThreads(const CustomUpdateWUInternal &cg)
{
    if (cg.getSynapseGroup()->getMatrixType() & SynapseMatrixWeight::KERNEL) {
        return cg.getSynapseGroup()->getKernelSizeFlattened();
    }
    else {
        return cg.getSynapseGroup()->getTrgNeuronGroup()->getNumNeurons();
    }
}
//--------------------------------------------------------------------------
void BackendSIMT::addPresynapticUpdateStrategy(PresynapticUpdateStrategySIMT::Base *strategy)
{
    s_PresynapticUpdateStrategies.push_back(strategy);
}
//--------------------------------------------------------------------------
void BackendSIMT::genNeuronPrevSpikeTimeUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged,
                                                     BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    const unsigned int batchSize = modelMerged.getModel().getBatchSize();

    // Parallelise over neuron groups
    idStart = 0;
    genParallelGroup<NeuronPrevSpikeTimeUpdateGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedNeuronPrevSpikeTimeUpdateGroups,
        [this](const NeuronGroupInternal &ng) { return padKernelSize(ng.getNumNeurons(), KernelNeuronUpdate); },
        [batchSize, this](EnvironmentExternalBase &popEnv, NeuronPrevSpikeTimeUpdateGroupMerged &ng)
        {
            CodeStream::Scope b(popEnv.getStream());

            // Create matching environment
            EnvironmentGroupMergedField<NeuronPrevSpikeTimeUpdateGroupMerged> neuronEnv(popEnv, ng);
            buildStandardEnvironment(neuronEnv, batchSize);

            // If neuron group requires delays
            if(ng.getArchetype().isDelayRequired()) {
                if(batchSize == 1) {
                    neuronEnv.printLine("const unsigned int lastTimestepDelaySlot = *$(_spk_que_ptr);");
                }
                else {
                    neuronEnv.printLine("const unsigned int lastTimestepDelaySlot = *$(_spk_que_ptr) + ($(batch) *  " + std::to_string(ng.getArchetype().getNumDelaySlots()) + ");");
                }
                neuronEnv.printLine("const unsigned int lastTimestepDelayOffset = lastTimestepDelaySlot * $(num_neurons);");

                if(ng.getArchetype().isPrevSpikeTimeRequired()) {
                    // If there is a spike for this thread, set previous spike time to time of last timestep
                    // **NOTE** spkQuePtr is updated below so this already points to last timestep
                    neuronEnv.print("if($(id) < $(_spk_cnt)[lastTimestepDelaySlot])");
                    {
                        CodeStream::Scope b(neuronEnv.getStream());
                        neuronEnv.printLine("$(_prev_spk_time)[lastTimestepDelayOffset + $(_spk)[lastTimestepDelayOffset + $(id)]] = $(t) - DT;");
                    }
                }
                if(ng.getArchetype().isPrevSpikeEventTimeRequired()) {
                    // If there is a spike-like-event for this thread, set previous spike-like-event time to time of last timestep
                    // **NOTE** spkQuePtr is updated below so this already points to last timestep
                    neuronEnv.print("if($(id) < $(_spk_cnt_envt)[lastTimestepDelaySlot])");
                    {
                        CodeStream::Scope b(neuronEnv.getStream());
                        neuronEnv.printLine("$(_prev_spk_evnt_time)[lastTimestepDelayOffset + $(_spk_evnt)[lastTimestepDelayOffset + $(id)]] = $(t) - DT;");
                    }
                }
            }
            // Otherwise
            else {
                if(batchSize > 1) {
                    neuronEnv.printLine("const unsigned int batchOffset = $(num_neurons) * $(batch);");
                }
                if(ng.getArchetype().isPrevSpikeTimeRequired()) {
                    // If there is a spike for this thread, set previous spike time to time of last timestep
                    neuronEnv.print("if($(id) < $(_spk_cnt)[$(batch)])");
                    {
                        CodeStream::Scope b(neuronEnv.getStream());
                        neuronEnv.print("$(_prev_spk_time)[");
                        if (batchSize == 1) {
                            neuronEnv.print("$(_spk)[$(id)]");
                        }
                        else {
                            neuronEnv.print("batchOffset + $(_spk)[batchOffset + $(id)]");
                        }
                        neuronEnv.printLine("] = $(t) - $(dt);");
                    }
                }
                if(ng.getArchetype().isPrevSpikeEventTimeRequired()) {
                    // If there is a spike-like-event for this thread, set previous spike-like-event time to time of last timestep
                    neuronEnv.print("if($(id) < $(_spk_cnt_evnt)[$(batch)])");
                    {
                        CodeStream::Scope b(neuronEnv.getStream());
                        neuronEnv.print("$(_prev_spk_evnt_time)[");
                        if (batchSize == 1) {
                            neuronEnv.print("$(_spk_evnt)[$(id)]");
                        }
                        else {
                            neuronEnv.print("batchOffset + $(_spk_evnt)[batchOffset + $(id)]");
                        }
                        neuronEnv.printLine("] = $(t) - $(dt);");
                    }
                }
            }
            neuronEnv.getStream() << std::endl;
        });

}
//--------------------------------------------------------------------------
void BackendSIMT::genNeuronSpikeQueueUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                                  BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    const unsigned int batchSize = modelMerged.getModel().getBatchSize();

    // Loop through local neuron groups
    idStart = 0;
    modelMerged.genMergedNeuronSpikeQueueUpdateGroups(
        *this, memorySpaces,
        [&env, &idStart, batchSize, this](auto &n)
        {
            if(idStart == 0) {
                env.getStream() << "if(id < " << n.getGroups().size() << ")";
            }
            else {
                env.getStream() << "if(id >= " << idStart << " && id < " << idStart + n.getGroups().size() << ")";
            }
            {
                CodeStream::Scope b(env.getStream());

                // Use this to get reference to merged group structure
                env.getStream() << getPointerPrefix() << "struct MergedNeuronSpikeQueueUpdateGroup" << n.getIndex() << " *group = &d_mergedNeuronSpikeQueueUpdateGroup" << n.getIndex() << "[id - " << idStart << "]; " << std::endl;
            
                // Create matching environment
                EnvironmentGroupMergedField<NeuronSpikeQueueUpdateGroupMerged> neuronEnv(env, n);
                buildStandardEnvironment(neuronEnv, batchSize);

                if(n.getArchetype().isDelayRequired()) { // with delay
                    neuronEnv.printLine("*$(_spk_que_ptr) = (*$(_spk_que_ptr) + 1) % " + std::to_string(n.getArchetype().getNumDelaySlots()) + ";");
                }

                if(batchSize > 1) {
                    neuronEnv.getStream() << "for(unsigned int batch = 0; batch < " << batchSize << "; batch++)" << CodeStream::OB(1);
                }
                n.genMergedGroupSpikeCountReset(neuronEnv, batchSize);
                if(batchSize > 1) {
                    neuronEnv.getStream() << CodeStream::CB(1);
                }
            }
            idStart += n.getGroups().size();
        });
}
//--------------------------------------------------------------------------
void BackendSIMT::genNeuronUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged,
                                        BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    const unsigned int batchSize = modelMerged.getModel().getBatchSize();

    // Generate code to zero shared memory spike event count using thread 1
    std::ostringstream shSpkCountInitStream;
    CodeStream shSpkCountInit(shSpkCountInitStream);
    shSpkCountInit << getSharedPrefix() << "unsigned int shSpkCount;" << std::endl;
    shSpkCountInit << "if (" << getThreadID() << " == 1)";
    {
        CodeStream::Scope b(shSpkCountInit);
        shSpkCountInit << "shSpkCount = 0;" << std::endl;
    }

    // Generate code to zero shared memory spike event count using thread 1
    std::ostringstream shSpkEvntCountInitStream;
    CodeStream shSpkEvntCountInit(shSpkEvntCountInitStream);
    shSpkEvntCountInit << getSharedPrefix() << "unsigned int shSpkEvntCount;" << std::endl;
    shSpkEvntCountInit << "if (" << getThreadID() << " == 1)";
    {
        CodeStream::Scope b(shSpkEvntCountInit);
        shSpkEvntCountInit << "shSpkEvntCount = 0;" << std::endl;
    }

    // Add shared memory substitutions so they're only instantiated as required
    EnvironmentExternal neuronEnv(env);
    const std::string blockSizeStr = std::to_string(getKernelBlockSize(KernelNeuronUpdate));
    neuronEnv.add(Type::Void, "_sh_spk", "shSpk",
                  {neuronEnv.addInitialiser(getSharedPrefix() + "unsigned int shSpk[" + blockSizeStr + "];")});
    neuronEnv.add(Type::Void, "_sh_spk_pos", "shSpkPos",
                  {neuronEnv.addInitialiser(getSharedPrefix() + "unsigned int shSpkPos;")});
    neuronEnv.add(Type::Void, "_sh_spk_count", "shSpkCount",
                  {neuronEnv.addInitialiser(shSpkCountInitStream.str())});
    neuronEnv.add(Type::Void, "_sh_spk_evnt", "shSpkEvnt",
                  {neuronEnv.addInitialiser(getSharedPrefix() + "unsigned int shSpkEvnt[" + blockSizeStr + "];")});
    neuronEnv.add(Type::Void, "_sh_spk_evnt_pos", "shSpkEvntPos",
                  {neuronEnv.addInitialiser(getSharedPrefix() + "unsigned int shSpkEvntPos;")});
    neuronEnv.add(Type::Void, "_sh_spk_evnt_count", "shSpkEvntCount",
                  {neuronEnv.addInitialiser(shSpkEvntCountInitStream.str())});

    // If any neuron groups record spikes
    if(std::any_of(modelMerged.getModel().getNeuronGroups().cbegin(), modelMerged.getModel().getNeuronGroups().cend(),
                   [](const auto &n) { return n.second.isSpikeRecordingEnabled(); }))
    {
        genRecordingSharedMemInit(env.getStream(), "");
    }

    // If any neuron groups record spike-like events
    if(std::any_of(modelMerged.getModel().getNeuronGroups().cbegin(), modelMerged.getModel().getNeuronGroups().cend(),
                   [](const auto &n) { return n.second.isSpikeEventRecordingEnabled(); }))
    {
        genRecordingSharedMemInit(env.getStream(), "Evnt");
    }

    genSharedMemBarrier(neuronEnv.getStream());

    // Parallelise over neuron groups
    idStart = 0;
    genParallelGroup<NeuronUpdateGroupMerged>(
        neuronEnv, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedNeuronUpdateGroups,
        [this](const NeuronGroupInternal &ng) { return padKernelSize(ng.getNumNeurons(), KernelNeuronUpdate); },
        [batchSize, &modelMerged, this](EnvironmentExternalBase &popEnv, NeuronUpdateGroupMerged &ng)
        {
            CodeStream::Scope b(popEnv.getStream());
            EnvironmentGroupMergedField<NeuronUpdateGroupMerged> groupEnv(popEnv, ng);
            buildStandardEnvironment(groupEnv, batchSize);
            
            // Call handler to generate generic neuron code
            groupEnv.print("if($(id) < $(num_neurons))");
            {
                CodeStream::Scope b(groupEnv.getStream());

                // Add population RNG field
                groupEnv.addField(getPopulationRNGType().createPointer(), "_rng", "rng",
                                  [this](const auto &g, size_t) { return getDeviceVarPrefix() + "rng" + g.getName(); },
                                  ng.getVarIndex(batchSize, VarAccessDuplication::DUPLICATE, "$(id)"));
                // **TODO** for OCL do genPopulationRNGPreamble(os, popSubs, "group->rng[" + ng.getVarIndex(batchSize, VarAccessDuplication::DUPLICATE, "$(id)") + "]") in initialiser

                ng.generateNeuronUpdate(*this, groupEnv, batchSize,
                                        // Emit true spikes
                                        [this](EnvironmentExternalBase &env, const NeuronUpdateGroupMerged &ng)
                                        {
                                            genEmitSpike(env, "", ng.getArchetype().isSpikeRecordingEnabled());
                                        },
                                        // Emit spike-like events
                                        [this](EnvironmentExternalBase &env, const NeuronUpdateGroupMerged &ng)
                                        {
                                            genEmitSpike(env, "_evnt", ng.getArchetype().isSpikeEventRecordingEnabled());
                                        });

                // Copy local stream back to local
                // **TODO** postamble for OCL
                //if(ng.getArchetype().isSimRNGRequired()) {
                //    genPopulationRNGPostamble(neuronEnv.getStream(), rng);
                //}
            }

            genSharedMemBarrier(groupEnv.getStream());

            // Use first thread to 'allocate' block of $(_spk) array for this block's spikes
            if(!ng.getArchetype().getNeuronModel()->getThresholdConditionCode().empty()) {
                groupEnv.getStream() << "if(" << getThreadID() << " == 0)";
                {
                    CodeStream::Scope b(groupEnv.getStream());
                    groupEnv.print("if ($(_sh_spk_count) > 0)");
                    {
                        CodeStream::Scope b(groupEnv.getStream());
                        groupEnv.print("$(_sh_spk_pos) = " + getAtomic(Type::Uint32) + "(&$(_spk_cnt)");
                        if(ng.getArchetype().isDelayRequired() && ng.getArchetype().isTrueSpikeRequired()) {
                            groupEnv.print("[*$(_spk_que_ptr)");
                            if(batchSize > 1) {
                                groupEnv.getStream() << " + (batch * " << ng.getArchetype().getNumDelaySlots() << ")";
                            }
                            groupEnv.printLine("], $(_sh_spk_count));");
                        }
                        else {
                            groupEnv.printLine("[$(batch)], $(_sh_spk_count));");
                        }
                    }
                } 
                genSharedMemBarrier(groupEnv.getStream());
            }

            // Use second thread to 'allocate' block of $(_spk_evnt) array for this block's spike-like events
            if(ng.getArchetype().isSpikeEventRequired()) {
                groupEnv.getStream() << "if (" << getThreadID() << " == 1)";
                {
                    CodeStream::Scope b(groupEnv.getStream());
                    groupEnv.print("if($(_sh_spk_evnt_count) > 0)");
                    {
                        CodeStream::Scope b(groupEnv.getStream());
                        groupEnv.print("$(_sh_spk_evnt_pos) = " + getAtomic(Type::Uint32) + "(&$(_spk_cnt_evnt)");
                        if(ng.getArchetype().isDelayRequired()) {
                            groupEnv.print("[*$(_spk_que_ptr)");
                            if(batchSize > 1) {
                                groupEnv.getStream() << " + (batch * " << ng.getArchetype().getNumDelaySlots() << ")";
                            }
                            groupEnv.printLine("], $(_sh_spk_evnt_count));");
                        }
                        else {
                            groupEnv.printLine("[$(batch)], $(_sh_spk_evnt_count));");
                        }
                    }
                } 
                genSharedMemBarrier(groupEnv.getStream());
            }

            // Copy spikes into block of $(_spk)
            const std::string queueOffset = ng.getWriteVarIndex(ng.getArchetype().isDelayRequired(), batchSize, VarAccessDuplication::DUPLICATE, "");
            if(!ng.getArchetype().getNeuronModel()->getThresholdConditionCode().empty()) {
                const std::string queueOffsetTrueSpk = ng.getWriteVarIndex(ng.getArchetype().isTrueSpikeRequired() && ng.getArchetype().isDelayRequired(), 
                                                                           batchSize, VarAccessDuplication::DUPLICATE, "");
                groupEnv.print("if(" + getThreadID() + " < $(_sh_spk_count))");
                {
                    CodeStream::Scope b(groupEnv.getStream());

                    groupEnv.printLine("const unsigned int n = $(_sh_spk)[" + getThreadID() + "];");

                    // Create new substition stack and explicitly replace id with 'n' and perform WU var update
                    EnvironmentExternal wuEnv(groupEnv);
                    wuEnv.add(Type::Uint32.addConst(), "id", "n");
                    ng.generateWUVarUpdate(*this, wuEnv, batchSize);

                    groupEnv.printLine("$(_spk)[" + queueOffsetTrueSpk + "$(_sh_spk_pos) + " + getThreadID() + "] = n;");
                    if(ng.getArchetype().isSpikeTimeRequired()) {
                        groupEnv.printLine("$(_spk_time)[" + queueOffset + "n] = $(t);");
                    }
                }
            }

            // Copy spike-like events into block of $(_spk_evnt)
            if(ng.getArchetype().isSpikeEventRequired()) {
                groupEnv.print("if(" + getThreadID() + " < $(_sh_spk_evnt_count))");
                {
                    CodeStream::Scope b(groupEnv.getStream());
                    groupEnv.printLine("const unsigned int n = $(_sh_spk_evnt)[" + getThreadID() + "];");

                    groupEnv.printLine("$(_spk_evnt)[" + queueOffset + "$(_sh_spk_evnt_pos) + " + getThreadID() + "] = n;");
                    if(ng.getArchetype().isSpikeEventTimeRequired()) {
                        groupEnv.printLine("$(_spk_evnt_time)[" + queueOffset + "n] = $(t);");
                    }
                }
            }

            // If we're recording spikes or spike-like events, use enough threads to copy this block's recording words
            if(ng.getArchetype().isSpikeRecordingEnabled() || ng.getArchetype().isSpikeEventRecordingEnabled()) {
                groupEnv.getStream() << "if(" << getThreadID() << " < " << m_KernelBlockSizes[KernelNeuronUpdate] / 32 << ")";
                {
                    CodeStream::Scope b(groupEnv.getStream());

                    // Calculate number of words which will be used to record this population's spikes in each batch
                    groupEnv.printLine("const unsigned int numRecordingWords = ($(num_neurons) + 31) / 32;");
                    groupEnv.printLine("const unsigned int popWordIdx = ($(id) / 32) + " + getThreadID() + ";");

                    // Build global index
                    std::string globalIndex = "(recordingTimestep * numRecordingWords * " + std::to_string(batchSize) + ") + popWordIdx";
                    if(batchSize > 1) {
                        globalIndex += " + (batch * numRecordingWords)";
                    }

                    groupEnv.getStream() << "if(popWordIdx < numRecordingWords)";
                    {
                        CodeStream::Scope c(groupEnv.getStream());
                        // If we are recording spikes, copy word to correct location in global memory
                        if(ng.getArchetype().isSpikeRecordingEnabled()) {
                            groupEnv.print("$(_record_spk)[" + globalIndex + "] = shSpkRecord");
                            if(m_KernelBlockSizes[KernelNeuronUpdate] != 32) {
                                groupEnv.print("[" + getThreadID() + "]");
                            }
                            groupEnv.printLine(";");
                        }

                        // If we are recording spike-like events, copy word to correct location in global memory
                        if(ng.getArchetype().isSpikeEventRecordingEnabled()) {
                            groupEnv.print("$(_record_spk_evnt)[" + globalIndex + "] = shSpkEvntRecord");
                            if(m_KernelBlockSizes[KernelNeuronUpdate] != 32) {
                                groupEnv.print("[" + getThreadID() + "]");
                            }
                            groupEnv.printLine(";");
                        }
                    }
                }
            }
        });
}
//--------------------------------------------------------------------------
void BackendSIMT::genSynapseDendriticDelayUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                                       BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    // Loop through merged synapse groups
    idStart = 0;
    modelMerged.genMergedSynapseDendriticDelayUpdateGroups(
        *this, memorySpaces,
        [&env, &idStart, &modelMerged, this](auto &sg)
        {
            env.getStream() << "// merged" << sg.getIndex() << std::endl;
            if(idStart == 0) {
                env.getStream() << "if(id < " << sg.getGroups().size() << ")";
            }
            else {
                env.getStream() << "if(id >= " << idStart << " && id < " << idStart + sg.getGroups().size() << ")";
            }
            {
                CodeStream::Scope b(env.getStream());

                // Use this to get reference to merged group structure
                env.getStream() << getPointerPrefix() << "struct MergedSynapseDendriticDelayUpdateGroup" << sg.getIndex() << " *group = &d_mergedSynapseDendriticDelayUpdateGroup" << sg.getIndex() << "[id - " << idStart << "]; " << std::endl;
                EnvironmentGroupMergedField<SynapseDendriticDelayUpdateGroupMerged> groupEnv(env, sg);
                buildStandardEnvironment(groupEnv, modelMerged.getModel().getBatchSize());
                groupEnv.printLine("*$(_den_delay_ptr) = (*$(_den_delay_ptr) + 1) % " + std::to_string(sg.getArchetype().getMaxDendriticDelayTimesteps()) + ";");
            }
            idStart += sg.getGroups().size();
        });
    env.getStream() << std::endl;
}
//--------------------------------------------------------------------------
void BackendSIMT::genPresynapticUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                             BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    EnvironmentExternal kernelEnv(env);

    // We need shLg if any synapse groups accumulate into shared memory
    // Determine the maximum shared memory outputs 
    size_t maxSharedMemPerThread = 0;
    for(const auto &s : modelMerged.getMergedPresynapticUpdateGroups()) {
        maxSharedMemPerThread = std::max(maxSharedMemPerThread,
                                         getPresynapticUpdateStrategy(s.getArchetype())->getSharedMemoryPerThread(s, *this));
    }

    // If any shared memory is required, declare array
    if(maxSharedMemPerThread > 0) {
        kernelEnv.getStream() << getSharedPrefix() <<" scalar shLg[" << maxSharedMemPerThread * getKernelBlockSize(KernelPresynapticUpdate) << "];" << std::endl;
    }

    // Shared memory for row length
    kernelEnv.add(Type::Void, "_sh_row_length", "shRowLength",
                  {kernelEnv.addInitialiser(getSharedPrefix() + "unsigned int shRowLength[" + std::to_string(getKernelBlockSize(KernelPresynapticUpdate)) + "];")});

    // Shared memory for spikes and spike events
    kernelEnv.add(Type::Void, "_sh_spk", "shSpk",
                  {kernelEnv.addInitialiser(getSharedPrefix() + "unsigned int shSpk[" + std::to_string(getKernelBlockSize(KernelPresynapticUpdate)) + "];")});
    kernelEnv.add(Type::Void, "_sh_spk_evnt", "shSpkEvnt",
                  {kernelEnv.addInitialiser(getSharedPrefix() + "unsigned int shSpkEvnt[" + std::to_string(getKernelBlockSize(KernelPresynapticUpdate)) + "];")});

    // Parallelise over synapse groups
    idStart = 0;
    genParallelGroup<PresynapticUpdateGroupMerged>(
        kernelEnv, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedPresynapticUpdateGroups,
        [this](const SynapseGroupInternal &sg) { return padKernelSize(getNumPresynapticUpdateThreads(sg, getPreferences()), KernelPresynapticUpdate); },
        [&modelMerged, this](EnvironmentExternalBase &env, PresynapticUpdateGroupMerged &sg)
        {
            EnvironmentGroupMergedField<PresynapticUpdateGroupMerged> groupEnv(env, sg);

            // Get presynaptic update strategy to use for this synapse group
            const auto *presynapticUpdateStrategy = getPresynapticUpdateStrategy(sg.getArchetype());
            LOGD_BACKEND << "Using '" << typeid(*presynapticUpdateStrategy).name() << "' presynaptic update strategy for merged synapse group '" << sg.getIndex() << "'";

            // Generate index calculation code
            const unsigned int batchSize = modelMerged.getModel().getBatchSize();
            buildStandardEnvironment(groupEnv, batchSize);

            // Generate preamble
            presynapticUpdateStrategy->genPreamble(groupEnv, sg, *this);

            // If spike events should be processed
            if(sg.getArchetype().isSpikeEventRequired()) {
                CodeStream::Scope b(groupEnv.getStream());
                presynapticUpdateStrategy->genUpdate(groupEnv, sg, *this, batchSize, false);
            }

            // If true spikes should be processed
            if(sg.getArchetype().isTrueSpikeRequired()) {
                CodeStream::Scope b(groupEnv.getStream());
                presynapticUpdateStrategy->genUpdate(groupEnv, sg, *this, batchSize, true);
            }

            groupEnv.getStream() << std::endl;

            // Generate pre-amble
            presynapticUpdateStrategy->genPostamble(groupEnv, sg, *this, batchSize);
        });
}
//--------------------------------------------------------------------------
void BackendSIMT::genPostsynapticUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                              BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    EnvironmentExternal kernelEnv(env);

    // Shared memory for column length and spikes
    kernelEnv.add(Type::Void, "_sh_col_length", "shColLength",
                  {kernelEnv.addInitialiser(getSharedPrefix() + "unsigned int shColLength[" + std::to_string(getKernelBlockSize(KernelPostsynapticUpdate)) + "];")});
    kernelEnv.add(Type::Void, "_sh_spk", "shSpk",
                  {kernelEnv.addInitialiser(getSharedPrefix() + "unsigned int shSpk[" + std::to_string(getKernelBlockSize(KernelPostsynapticUpdate)) + "];")});

    // Parallelise over postsynaptic update groups
    idStart = 0;
    genParallelGroup<PostsynapticUpdateGroupMerged>(
        kernelEnv, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedPostsynapticUpdateGroups,
        [this](const SynapseGroupInternal &sg) { return padKernelSize(getNumPostsynapticUpdateThreads(sg), KernelPostsynapticUpdate); },
        [&modelMerged, this](EnvironmentExternalBase &env, PostsynapticUpdateGroupMerged &sg)
        {
            EnvironmentGroupMergedField<PostsynapticUpdateGroupMerged> groupEnv(env, sg);

            // Generate index calculation code
            const unsigned int batchSize = modelMerged.getModel().getBatchSize();
            buildStandardEnvironment(groupEnv, batchSize);

            groupEnv.printLine("const unsigned int numSpikes = $(_trg_spk_cnt)[" + sg.getPostSlot(batchSize) + "];");
            
            groupEnv.getStream() << "const unsigned int numSpikeBlocks = (numSpikes + " << getKernelBlockSize(KernelPostsynapticUpdate) - 1 << ") / " << getKernelBlockSize(KernelPostsynapticUpdate) << ";" << std::endl;
            groupEnv.getStream() << "for (unsigned int r = 0; r < numSpikeBlocks; r++)";
            {
                CodeStream::Scope b(groupEnv.getStream());
                groupEnv.getStream() << "const unsigned int numSpikesInBlock = (r == numSpikeBlocks - 1) ? ((numSpikes - 1) % " << getKernelBlockSize(KernelPostsynapticUpdate) << ") + 1 : " << getKernelBlockSize(KernelPostsynapticUpdate) << ";" << std::endl;

                groupEnv.getStream() << "if (" << getThreadID() << " < numSpikesInBlock)";
                {
                    CodeStream::Scope b(groupEnv.getStream());
                    const std::string index = "(r * " + std::to_string(getKernelBlockSize(KernelPostsynapticUpdate)) + ") + " + getThreadID();
                    groupEnv.printLine("const unsigned int spk = $(_trg_spk)[" + sg.getPostVarIndex(batchSize, VarAccessDuplication::DUPLICATE, index) + "];");
                    groupEnv.getStream() << "shSpk[" << getThreadID() << "] = spk;" << std::endl;

                    if(sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                        groupEnv.getStream() << "shColLength[" << getThreadID() << "] = group->colLength[spk];" << std::endl;
                    }
                }

                genSharedMemBarrier(groupEnv.getStream());
                groupEnv.getStream() << "// only work on existing neurons" << std::endl;
                groupEnv.print("if ($(id) < $(_col_stride))");
                {
                    CodeStream::Scope b(groupEnv.getStream());
                    groupEnv.getStream() << "// loop through all incoming spikes for learning" << std::endl;
                    groupEnv.getStream() << "for (unsigned int j = 0; j < numSpikesInBlock; j++)";
                    {
                        CodeStream::Scope b(groupEnv.getStream());

                        if (sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                            groupEnv.print("if ($(id) < $(_sh_col_length)[j])");
                            groupEnv.getStream() << CodeStream::OB(1540);
                        }

                        EnvironmentGroupMergedField<PostsynapticUpdateGroupMerged> synEnv(groupEnv, sg);
                        if (sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                            synEnv.add(Type::Uint32.addConst(), "id_syn", "synAddress",
                                       {synEnv.addInitialiser("const unsigned int synAddress = $(_remap)[($(_sh_spk)[j] * $(_col_stride)) + $(id)];")});

                            // **OPTIMIZE** we can do a fast constant divide optimization here
                            synEnv.add(Type::Uint32.addConst(), "id_pre", "idPre",
                                       {synEnv.addInitialiser("const unsigned int idPre = $(synEnv) / $(_row_stride);")});
                        }
                        else {
                            synEnv.add(Type::Uint32.addConst(), "id_syn", "synAddress",
                                       {synEnv.addInitialiser("const unsigned int synAddress = ($(id) * $(num_post)) + $(_sh_spk)[j];")});

                            synEnv.add(Type::Uint32.addConst(), "id_pre", "$(id)");
                        }

                        synEnv.add(Type::Uint32.addConst(), "id_post", "$(_sh_spk)[j]");

                        synEnv.add(Type::AddToPre, "addToPre", getAtomic(modelMerged.getModel().getPrecision()) + "(&$(_out_pre)[" + sg.getPreISynIndex(batchSize, "$(id_pre)") + "], $(0))");

                        sg.generateSynapseUpdate(*this, synEnv, batchSize);

                        if (sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                            synEnv.getStream() << CodeStream::CB(1540);
                        }
                    }
                }
            }
        }
    );
}
//--------------------------------------------------------------------------
void BackendSIMT::genSynapseDynamicsKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                           BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    // Parallelise over synapse groups whose weight update models have code for synapse dynamics
    idStart = 0;
    genParallelGroup<SynapseDynamicsGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedSynapseDynamicsGroups,
        [this](const SynapseGroupInternal &sg) { return padKernelSize(getNumSynapseDynamicsThreads(sg), KernelSynapseDynamicsUpdate); },
        [&modelMerged, this](EnvironmentExternalBase &env, SynapseDynamicsGroupMerged &sg)
        {
            EnvironmentGroupMergedField<SynapseDynamicsGroupMerged> groupEnv(env, sg);

            // Generate index calculation code
            const unsigned int batchSize = modelMerged.getModel().getBatchSize();
            buildStandardEnvironment(groupEnv, batchSize);

            if(sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                groupEnv.print("if ($(id) < ($(num_pre) * $(_row_stride)))");
            }
            else {
                groupEnv.print("if ($(id( < ($(num_pre) * $(num_post)))");
            }
            {
                CodeStream::Scope b(groupEnv.getStream());
                EnvironmentGroupMergedField<SynapseDynamicsGroupMerged> synEnv(groupEnv, sg);

                if(sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                    // **OPTIMIZE * *we can do a fast constant divide optimization here and use the result to calculate the remainder
                    synEnv.printLine("const unsigned int row = $(id) / $(_row_stride);");
                    synEnv.printLine("const unsigned int col = $(id) % $(_row_stride);");

                    synEnv.add(Type::Uint32.addConst(), "id_pre", "row");
                    synEnv.add(Type::Uint32.addConst(), "id_post", "$(_ind)[$(id)]");

                    synEnv.getStream() << "if(col < " << synEnv["_row_length"] << "[row])";
                    synEnv.getStream() << CodeStream::OB(1);
                }
                else {
                    // **OPTIMIZE** we can do a fast constant divide optimization here and use the result to calculate the remainder
                    synEnv.add(Type::Uint32.addConst(), "id_pre", "idPre",
                               {synEnv.addInitialiser("const unsigned int idPre = ($(id) / $(_row_stride))")});
                    synEnv.add(Type::Uint32.addConst(), "id_post", "idPost",
                               {synEnv.addInitialiser("const unsigned int idPost = ($(id) % $(_row_stride)")});    
                }

                synEnv.add(Type::Uint32.addConst(), "id_syn", "$(id)");

                synEnv.add(Type::AddToPostDenDelay, "addToPostDelay", 
                           getAtomic(modelMerged.getModel().getPrecision()) + "(&$(_den_delay)[" + sg.getPostDenDelayIndex(batchSize, "$(id_post)", "$(1)") + "], $(0))");
                synEnv.add(Type::AddToPost, "addToPost", 
                           getAtomic(modelMerged.getModel().getPrecision()) + "(&$(_out_post)[" + sg.getPostISynIndex(batchSize, "$(id_post)") + "], $(0))");
                synEnv.add(Type::AddToPre, "addToPre",
                            getAtomic(modelMerged.getModel().getPrecision()) + "(&$(_out_pre)[" + sg.getPreISynIndex(batchSize, "$(id_pre)") + "], $(0))");
                
                sg.generateSynapseUpdate(*this, synEnv, batchSize);

                if (sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                    synEnv.getStream() << CodeStream::CB(1);
                }
            }
        });
}
//--------------------------------------------------------------------------
void BackendSIMT::genCustomUpdateKernel(EnvironmentExternal &env, ModelSpecMerged &modelMerged, 
                                        BackendBase::MemorySpaces &memorySpaces, const std::string &updateGroup, size_t &idStart) const
{
    const unsigned int batchSize = modelMerged.getModel().getBatchSize();
    genParallelGroup<CustomUpdateGroupMerged>(
        env, modelMerged, memorySpaces, updateGroup, idStart, &ModelSpecMerged::genMergedCustomUpdateGroups,
        [batchSize, this](const CustomUpdateInternal &cu) { return getPaddedNumCustomUpdateThreads(cu, batchSize); },
        [batchSize, this](EnvironmentExternalBase &env, CustomUpdateGroupMerged &cg)
        {
            const size_t blockSize = getKernelBlockSize(KernelCustomUpdate);
            
            // If update is a batch reduction
            if(cg.getArchetype().isBatchReduction()) {
                env.getStream() << "// only do this for existing neurons" << std::endl;
                env.getStream() << "if(" << env["id"] << " < group->size)";
                {
                    CodeStream::Scope b(env.getStream());
                    EnvironmentGroupMergedField<CustomUpdateGroupMerged> groupEnv(env, cg);

                    // Initialise reduction targets
                    const auto reductionTargets = genInitReductionTargets(groupEnv.getStream(), cg, groupEnv["id"]);

                    // Loop through batches
                    // **TODO** this naive approach is good for reduction when there are lots of neurons/synapses but,
                    // if this isn't the case (TF uses a threshold of 4096), we should do something smarter
                    groupEnv.getStream() << "for(unsigned int batch = 0; batch < " << batchSize << "; batch++)";
                    {
                        CodeStream::Scope b(groupEnv.getStream());
                        groupEnv.add(Type::Uint32.addConst(), "batch", "batch");

                        buildStandardEnvironment(groupEnv);
                        
                        // **THINK** it would be great to 'lift' reads of SHARED variables out of this loop
                        cg.generateCustomUpdate(*this, groupEnv);

                        // Loop through reduction targets and generate reduction
                        for(const auto &r : reductionTargets) {
                            groupEnv.getStream() << getReductionOperation("lr" + r.name, "l" + r.name, r.access, r.type) << ";" << std::endl;
                        }
                    }

                    // Loop through reduction targets and write reduced value back to memory
                    for(const auto &r : reductionTargets) {
                        groupEnv.getStream() << "group->" << r.name << "[" << r.index << "] = lr" << r.name << ";" << std::endl;
                    }
                }
            }
            // Otherwise, if this is a neuron reduction
            else if (cg.getArchetype().isNeuronReduction()) {
                env.getStream() << "// only do this for existing neurons" << std::endl;
                env.getStream() << "if(" << env["id"] << " < " << (32 * batchSize) << ")";
                {
                    CodeStream::Scope b(env.getStream());
                    EnvironmentGroupMergedField<CustomUpdateGroupMerged> groupEnv(env, cg);

                    // Split ID into lane and batch
                    groupEnv.getStream() << "const unsigned int lane = " << env["id"] << " % 32;" << std::endl;
                    groupEnv.getStream() << "const unsigned int batch = " << env["id"] << " / 32;" << std::endl;
                    groupEnv.add(Type::Uint32.addConst(), "batch", "batch");

                    buildStandardEnvironment(groupEnv);

                    // Initialise reduction targets
                    const auto reductionTargets = genInitReductionTargets(groupEnv.getStream(), cg);

                    // Loop through warps of data
                    // **TODO** this approach is good for reductions where there are small numbers of neurons but large batches sizes but,
                    // if this isn't the case (TF uses a threshold of 1024), we should do something smarter
                    groupEnv.getStream() << "for(unsigned int idx = lane; idx < " << groupEnv["size"] << "; idx += 32)";
                    {
                        CodeStream::Scope b(groupEnv.getStream());

                        // Re-substitute id with loop index
                        groupEnv.add(Type::Uint32.addConst(), "id", "idx");

                        // **THINK** it would be great to 'lift' reads of NEURON_SHARED variables out of this loop
                        cg.generateCustomUpdate(*this, groupEnv);

                        // Loop through reduction targets and generate reduction
                        for (const auto &r : reductionTargets) {
                            groupEnv.getStream() << getReductionOperation("lr" + r.name, "l" + r.name, r.access, r.type) << ";" << std::endl;
                        }
                    }

                    // Perform warp reduction into first lane
                    // **YUCK** CUDA-specific
                    for (unsigned int i = 16; i > 0; i /= 2) {
                        for (const auto &r : reductionTargets) {
                            groupEnv.getStream() << getReductionOperation("lr" + r.name, "__shfl_down_sync(0xFFFFFFFF, lr" + r.name + ", " + std::to_string(i) + ")",
                                                                          r.access, r.type) << ";" << std::endl;
                        }
                    }

                    // In first lane, loop through reduction targets and write reduced value back to memory
                    groupEnv.getStream() << "if(lane == 0)";
                    {
                        CodeStream::Scope b(groupEnv.getStream());
                        for (const auto &r : reductionTargets) {
                            groupEnv.getStream() << "group->" << r.name << "[" << r.index << "] = lr" << r.name << ";" << std::endl;
                        }
                    }
                }
            }
            // Otherwise, if this update isn't per-neuron
            else if (!cg.getArchetype().isPerNeuron()) {
                EnvironmentGroupMergedField<CustomUpdateGroupMerged> groupEnv(env, cg);
                if(cg.getArchetype().isBatched()) {
                    groupEnv.add(Type::Uint32.addConst(), "batch", "$(id)");
                    groupEnv.add(Type::Uint32.addConst(), "id", "0");
                }
                // Otherwise, just substitute "batch" for 0
                else {
                    groupEnv.add(Type::Uint32.addConst(), "batch", "0");
                }

                groupEnv.getStream() << "// only do this for existing neurons" << std::endl;
                groupEnv.getStream() << "if(" << groupEnv["batch"] << " < " << (cg.getArchetype().isBatched() ? batchSize : 1) << ")";
                {
                    CodeStream::Scope b(groupEnv.getStream());

                    buildStandardEnvironment(groupEnv);
                    cg.generateCustomUpdate(*this, groupEnv);
                }
            }
            // Otherwise
            else {
                EnvironmentGroupMergedField<CustomUpdateGroupMerged> groupEnv(env, cg);

                if(cg.getArchetype().isBatched()) {
                    // Split ID into intra-batch ID and batch
                    // **TODO** fast-divide style optimisations here
                    const std::string blockSizeStr = std::to_string(blockSize);
                    const size_t paddedSizeInit = groupEnv.addInitialiser("const unsigned int paddedSize = " + blockSizeStr + " * (($(size) + " + blockSizeStr + " - 1) / " + blockSizeStr + ");");
    
                    // Replace id in substitution with intra-batch ID and add batch
                    groupEnv.add(Type::Uint32.addConst(), "id", "bid",
                                 {paddedSizeInit, groupEnv.addInitialiser("const unsigned int bid = $(id) % paddedSize;")});
                    groupEnv.add(Type::Uint32.addConst(), "batch", "batch",
                                 {paddedSizeInit, groupEnv.addInitialiser("const unsigned int batch = $(id) / paddedSize;")});
                }
                // Otherwise, just substitute "batch" for 0
                else {
                    groupEnv.add(Type::Uint32.addConst(), "batch", "0");
                }

                groupEnv.getStream() << "// only do this for existing neurons" << std::endl;
                groupEnv.print("if($(id) < $(size))");
                {
                    CodeStream::Scope b(groupEnv.getStream());

                    buildStandardEnvironment(groupEnv);
                    cg.generateCustomUpdate(*this, groupEnv);
                }
            }
        });
}
//--------------------------------------------------------------------------
void BackendSIMT::genCustomUpdateWUKernel(EnvironmentExternal &env, ModelSpecMerged &modelMerged,
                                          BackendBase::MemorySpaces &memorySpaces, const std::string &updateGroup, size_t &idStart) const
{
    const unsigned int batchSize = modelMerged.getModel().getBatchSize();
    genParallelGroup<CustomUpdateWUGroupMerged>(
        env, modelMerged, memorySpaces, updateGroup, idStart, &ModelSpecMerged::genMergedCustomUpdateWUGroups,
        [batchSize, this](const CustomUpdateWUInternal &cu) { return getPaddedNumCustomUpdateWUThreads(cu, batchSize); },
        [batchSize, this](EnvironmentExternalBase &env, CustomUpdateWUGroupMerged &cg)
        {
            const SynapseGroupInternal *sg = cg.getArchetype().getSynapseGroup();
            const size_t blockSize = getKernelBlockSize(KernelCustomUpdate);

            // Calculate size of each batch to update
            if (sg->getMatrixType() & SynapseMatrixWeight::KERNEL) {
                // Loop through kernel dimensions and multiply together
                env.getStream() << "const unsigned int size = ";
                for (size_t i = 0; i < sg->getKernelSize().size(); i++) {
                    env.print(getKernelSize(cg, i));
                    if (i != (sg->getKernelSize().size() - 1)) {
                        env.getStream() << " * ";
                    }
                }
                env.getStream() << ";" << std::endl;
            }
            else {
                env.printLine("const unsigned int size = $(num_pre) * $(_row_stride);");
            }

            // If update isn't a batch reduction
            EnvironmentGroupMergedField<CustomUpdateWUGroupMerged> groupEnv(env, cg);
            if(!cg.getArchetype().isBatchReduction()) {
                // If it's batched
                if(cg.getArchetype().isBatched()) {
                   // Split ID into intra-batch ID and batch
                    // **TODO** fast-divide style optimisations here
                    const std::string blockSizeStr = std::to_string(blockSize);
                    const size_t paddedSizeInit = groupEnv.addInitialiser("const unsigned int paddedSize = " + blockSizeStr + " * ((size + " + blockSizeStr + " - 1) / " + blockSizeStr + ");");
    
                    // Replace id in substitution with intra-batch ID and add batch
                    groupEnv.add(Type::Uint32.addConst(), "id", "bid",
                                 {paddedSizeInit, groupEnv.addInitialiser("const unsigned int bid = $(id) % paddedSize;")});
                    groupEnv.add(Type::Uint32.addConst(), "batch", "batch",
                                 {paddedSizeInit, groupEnv.addInitialiser("const unsigned int batch = $(id) / paddedSize;")});
                    groupEnv.add(Type::Uint32.addConst(), "_batch_offset", "batchOffset",
                                 {groupEnv.addInitialiser("const unsigned int batchOffset = size * $(batch);")});
                }
                // Otherwise, just substitute "batch" for 0
                else {
                    groupEnv.add(Type::Uint32.addConst(), "batch", "0");
                }
            }

            // if this isn't a padding thread
            groupEnv.getStream() << "if (" << groupEnv["id"] << " < size)";
            {
                CodeStream::Scope b(groupEnv.getStream());
                EnvironmentGroupMergedField<CustomUpdateWUGroupMerged> synEnv(groupEnv, cg);

                if (sg->getMatrixType() & SynapseMatrixWeight::KERNEL) {
                    synEnv.add(Type::Uint32.addConst(), "id_syn", "$(id)");
                    synEnv.add(Type::Uint32.addConst(), "id_kernel", "$(id)");
                }
                else {
                    if (sg->getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                        // **OPTIMIZE * *we can do a fast constant divide optimization here and use the result to calculate the remainder
                        synEnv.printLine("const unsigned int row = $(id) / $(_row_stride);");
                        synEnv.printLine("const unsigned int col = $(id) % $(_row_stride);");

                        synEnv.add(Type::Uint32.addConst(), "id_pre", "row");
                        synEnv.add(Type::Uint32.addConst(), "id_post", "$(_ind)[$(id)]");
                        
                        synEnv.print("if(col < $(_row_length)[row])");
                        synEnv.getStream() << CodeStream::OB(2);
                    }
                    else {
                        // **OPTIMIZE** we can do a fast constant divide optimization here and use the result to calculate the remainder
                        synEnv.add(Type::Uint32.addConst(), "id_pre", "idPre",
                                   {synEnv.addInitialiser("const unsigned int idPre = $(id) / $(_row_stride)")});
                        synEnv.add(Type::Uint32.addConst(), "id_post", "idPost",
                                   {synEnv.addInitialiser("const unsigned int idPost = $(id) % $(_row_stride)")});
                    }
                }

                synEnv.add(Type::Uint32.addConst(), "id_syn", "$(id)");

                // Initialise reduction targets
                const auto reductionTargets = genInitReductionTargets(synEnv.getStream(), cg, synEnv["id_syn"]);

                // If this is a reduction
                if(cg.getArchetype().isBatchReduction()) {
                    // Loop through batches
                    // **TODO** this naive approach is good for reduction when there are lots of neurons/synapses but,
                    // if this isn't the case (TF uses a threshold of 4096), we should do something smarter
                    synEnv.getStream() << "for(unsigned int batch = 0; batch < " << batchSize << "; batch++)";
                    synEnv.getStream() << CodeStream::OB(1);
                    synEnv.add(Type::Uint32.addConst(), "batch", "batch");
                }

                cg.generateCustomUpdate(*this, synEnv);

                // If this is a reduction
                if(cg.getArchetype().isBatchReduction()) {
                    // Loop through reduction targets and generate reduction
                    for(const auto &r : reductionTargets) {
                        synEnv.getStream() << getReductionOperation("lr" + r.name, "l" + r.name, r.access, r.type) << ";" << std::endl;
                    }

                    // End for loop through batches
                    synEnv.getStream() << CodeStream::CB(1);

                    // Loop through reduction targets and write reduced value back to memory
                    for(const auto &r : reductionTargets) {
                        synEnv.getStream() << "group->" << r.name << "[" << r.index << "] = lr" << r.name << ";" << std::endl;
                    }
                }

                if (sg->getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                    synEnv.getStream() << CodeStream::CB(2);
                }
            }
        });
}
//--------------------------------------------------------------------------
void BackendSIMT::genCustomTransposeUpdateWUKernel(EnvironmentExternal &env, ModelSpecMerged &modelMerged,
                                                   BackendBase::MemorySpaces &memorySpaces, const std::string &updateGroup, size_t &idStart) const
{
    // Generate 2D array
    const size_t blockSize = getKernelBlockSize(KernelCustomTransposeUpdate);
    env.getStream() << getSharedPrefix() << " float shTile[" << blockSize << "][" << (blockSize + 1) << "];" << std::endl;
    genParallelGroup<CustomUpdateTransposeWUGroupMerged>(
        env, modelMerged, memorySpaces, updateGroup, idStart, &ModelSpecMerged::genMergedCustomUpdateTransposeWUGroups,
        [&modelMerged, this](const CustomUpdateWUInternal &cu) { return getPaddedNumCustomUpdateTransposeWUThreads(cu, modelMerged.getModel().getBatchSize()); },
        [blockSize, this](EnvironmentExternalBase &env, CustomUpdateTransposeWUGroupMerged &cg)
        {
            EnvironmentGroupMergedField<CustomUpdateTransposeWUGroupMerged> groupEnv(env, cg);

            // Get index of variable being transposed
            const size_t transposeVarIdx = std::distance(cg.getArchetype().getVarReferences().cbegin(),
                                                         std::find_if(cg.getArchetype().getVarReferences().cbegin(), cg.getArchetype().getVarReferences().cend(),
                                                                      [](const auto &v) { return v.second.getTransposeSynapseGroup() != nullptr; }));
            const std::string transposeVarName = cg.getArchetype().getCustomUpdateModel()->getVarRefs().at(transposeVarIdx).name;

            // To allow these kernels to be batched, we turn 2D grid into wide 1D grid of 2D block so calculate size
            groupEnv.getStream() << "const unsigned int numXBlocks = (" << groupEnv["num_post"] << " + " << (blockSize - 1) << ") / " << blockSize << ";" << std::endl;

            // Calculate what block this kernel starts at (because of kernel merging, it may not start at block 0)
            groupEnv.getStream() << "const unsigned int blockStart = " << groupEnv["_group_start_id"] << " / " << blockSize << ";" << std::endl;

            if(cg.getArchetype().isBatched()) {
                // If there's multiple batches we also need to know how many Y blocks and hence total blocks there are
                groupEnv.getStream() << "const unsigned int numYBlocks = (" << groupEnv["num_pre"] << " + " << (blockSize - 1) << ") / " << blockSize << ";" << std::endl;
                groupEnv.getStream() << "const unsigned int numBlocks = numXBlocks * numYBlocks;" << std::endl;

                // Therefore determine block and batch
                groupEnv.getStream() << "const unsigned int batchBlock = " << getBlockID(0) << " - blockStart;" << std::endl;
                groupEnv.getStream() << "const unsigned int block = batchBlock % numBlocks;" << std::endl;
                groupEnv.getStream() << "const unsigned int batch = batchBlock / numBlocks;" << std::endl;

                // Finally, calculate batch offset into arrays etc
                groupEnv.printLine("const unsigned int batchOffset = batch * $(num_pre) * $(num_post);");

                // Add batch to substitutions
                groupEnv.add(Type::Uint32.addConst(), "batch", "batch");
            }
            // Otherwise, just substitute "batch" for 0
            else {
                groupEnv.getStream() << "const unsigned int block = " << getBlockID(0) << " - blockStart;" << std::endl;
                groupEnv.add(Type::Uint32.addConst(), "batch", "0");
            }

            // Divide block index into x and y
            // **TODO** fast-divide style optimisations here
            groupEnv.getStream() << "const unsigned int blockX = (block % numXBlocks);" << std::endl;
            groupEnv.getStream() << "const unsigned int blockY = (block / numXBlocks);" << std::endl;

            {
                CodeStream::Scope b(groupEnv.getStream());
                groupEnv.getStream() << "// Calculate coordinate of thread in input matrix" << std::endl;
                groupEnv.getStream() << "const unsigned int x = (blockX * " << blockSize << ") + " << getThreadID(0) << ";" << std::endl;
                groupEnv.getStream() << "const unsigned int y = (blockY * " << blockSize << ") + " << getThreadID(1) << ";" << std::endl;

                groupEnv.getStream() << "// If thread isn't off the 'right' edge of the input matrix" << std::endl;
                groupEnv.getStream() << "if(x < " << groupEnv["num_post"] << ")";
                {
                    CodeStream::Scope b(groupEnv.getStream());
                    groupEnv.getStream() << "// Loop through input rows " << std::endl;
                    groupEnv.getStream() << "for (unsigned int j = 0; j < " << blockSize << "; j += 8)";
                    {
                        CodeStream::Scope b(groupEnv.getStream());
                        groupEnv.getStream() << "// If thread isn't off the 'bottom' edge of the input matrix" << std::endl;
                        groupEnv.getStream() << "if((y + j) < " << groupEnv["num_pre"] << ")";
                        {
                            CodeStream::Scope b(groupEnv.getStream());
                            EnvironmentGroupMergedField<CustomUpdateTransposeWUGroupMerged> synEnv(groupEnv, cg);

                            synEnv.add(Type::Uint32.addConst(), "id_pre", "y");
                            synEnv.add(Type::Uint32.addConst(), "id_post", "x");
                            synEnv.add(Type::Uint32.addConst(), "id_syn", "idx",
                                       {synEnv.addInitialiser("const unsigned int idx = ((y + j) * $(num_post)) + x;")});
                            cg.generateCustomUpdate(*this, synEnv);

                            // Write forward weight to shared memory
                            synEnv.getStream() << "shTile[" << getThreadID(1) << " + j][" << getThreadID(0) << "] = l" << transposeVarName << ";" << std::endl;
                        }
                    }
                }
            }
            genSharedMemBarrier(env.getStream());
            {
                CodeStream::Scope b(groupEnv.getStream());
                groupEnv.getStream() << "// Calculate (transposed) coordinate of thread in output matrix" << std::endl;
                groupEnv.getStream() << "const unsigned int x = (blockY * " << blockSize << ") + " << getThreadID(0) << ";" << std::endl;
                groupEnv.getStream() << "const unsigned int y = (blockX * " << blockSize << ") + " << getThreadID(1) << ";" << std::endl;

                groupEnv.getStream() << "// If thread isn't off the 'bottom' edge of the output matrix" << std::endl;
                groupEnv.getStream() << "if(x < " << groupEnv["num_pre"] << ")";
                {
                    CodeStream::Scope b(groupEnv.getStream());
                    groupEnv.getStream() << "// Loop through output rows" << std::endl;
                    groupEnv.getStream() <<  "for(unsigned int j = 0; j < " << blockSize << "; j += 8)";
                    {
                        CodeStream::Scope b(groupEnv.getStream());
                        groupEnv.getStream() << "// If thread isn't off the 'right' edge of the output matrix" << std::endl;
                        groupEnv.getStream() << "if((y + j) < group" << groupEnv["num_post"] << ")";
                        {
                            CodeStream::Scope b(groupEnv.getStream());
                            groupEnv.getStream() << "group->" << transposeVarName << "Transpose[";
                            if(cg.getArchetype().isBatched()) {
                                groupEnv.getStream() << "batchOffset + ";
                            }
                            groupEnv.getStream() << "((y + j) * " << groupEnv["num_pre"] << ") + x] = shTile[" << getThreadID(0) << "][" << getThreadID(1) << " + j];" << std::endl;
                        }
                    }
                }
            }
        });
}
//--------------------------------------------------------------------------
void BackendSIMT::genCustomConnectivityUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged,
                                                    BackendBase::MemorySpaces &memorySpaces, const std::string &updateGroup, size_t &idStart) const
{
    // Parallelise across presynaptic neurons
    genParallelGroup<CustomConnectivityUpdateGroupMerged>(
        env, modelMerged, memorySpaces, updateGroup, idStart, &ModelSpecMerged::genMergedCustomConnectivityUpdateGroups,
        [this](const CustomConnectivityUpdateInternal &cg) { return padSize(cg.getSynapseGroup()->getSrcNeuronGroup()->getNumNeurons(), KernelCustomUpdate); },
        [&modelMerged, this](EnvironmentExternalBase &env, CustomConnectivityUpdateGroupMerged &cg)
        {
            EnvironmentGroupMergedField<CustomConnectivityUpdateGroupMerged> groupEnv(env, cg);
            
            buildStandardEnvironment(groupEnv);

            groupEnv.getStream() << "// only do this for existing presynaptic neurons" << std::endl;
            groupEnv.print("if($(id) < $(num_pre))");
            {
                CodeStream::Scope b(groupEnv.getStream());

                // Configure substitutions
                groupEnv.add(Type::Uint32.addConst(), "id_pre", "$(id)");
                
                // Copy global RNG stream to local and use pointer to this for rng
                const std::string rng = printSubs("$(_rng)[$(id)]", groupEnv);
                if(Utils::isRNGRequired(cg.getArchetype().getRowUpdateCodeTokens())) {
                    groupEnv.add(Type::Void, "rng", genPopulationRNGPreamble(groupEnv.getStream(), rng));
                }

                cg.generateUpdate(*this, groupEnv, modelMerged.getModel().getBatchSize());
                
                // Copy local stream back to local
                if(Utils::isRNGRequired(cg.getArchetype().getRowUpdateCodeTokens())) {
                    genPopulationRNGPostamble(groupEnv.getStream(), rng);
                }
            }
        });
}
//--------------------------------------------------------------------------
void BackendSIMT::genInitializeKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                      BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    env.getStream() << "// ------------------------------------------------------------------------" << std::endl;
    env.getStream() << "// Local neuron groups" << std::endl;
    idStart = 0;
    const unsigned int batchSize = modelMerged.getModel().getBatchSize();
    genParallelGroup<NeuronInitGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedNeuronInitGroups,
        [this](const NeuronGroupInternal &ng) { return padKernelSize(ng.getNumNeurons(), KernelInitialize); },
        [&modelMerged, batchSize, this](EnvironmentExternalBase &env, NeuronInitGroupMerged &ng)
        {
            EnvironmentGroupMergedField<NeuronInitGroupMerged> groupEnv(env, ng);
            buildStandardEnvironment(groupEnv, batchSize);

            groupEnv.getStream() << "// only do this for existing neurons" << std::endl;
            groupEnv.print("if($(id) < $(num_neurons))");
            {
                CodeStream::Scope b(groupEnv.getStream());

                // If population RNGs are initialised on device and this neuron is going to require one, 
                
                if(isPopulationRNGInitialisedOnDevice() && ng.getArchetype().isSimRNGRequired()) {
                    // Add field for RNG
                    EnvironmentGroupMergedField<NeuronInitGroupMerged> rngInitEnv(groupEnv, ng);
                    rngInitEnv.addField(getPopulationRNGType().createPointer(), "_rng", "rng",
                                        [this](const auto &g, size_t) { return getDeviceVarPrefix() + "rng" + g.getName(); });

                    // If batch size is 1, initialise single RNG using GLOBAL thread id for sequence
                    if(batchSize == 1) {
                        genPopulationRNGInit(rngInitEnv.getStream(), printSubs("$(_rng)[$(id)]", rngInitEnv), 
                                             "deviceRNGSeed", "id");
                    }
                    // Otherwise, loop through batches and initialise independent RNGs using GLOBAL thread id as basis of sequence
                    else {
                        env.getStream() << "for(unsigned int b = 0; b < " << batchSize << "; b++)";
                        {
                            CodeStream::Scope b(rngInitEnv.getStream());
                            genPopulationRNGInit(rngInitEnv.getStream(), printSubs("$(_rng)[(b * $(num_neurons)) + $(id)]", rngInitEnv), 
                                                 "deviceRNGSeed", "(b * " + std::to_string(getNumInitialisationRNGStreams(modelMerged)) + ") + id");
                        }
                    }
                    
                }

                // If this neuron requires an RNG for initialisation,
                // make copy of global phillox RNG and skip ahead by thread id
                // **NOTE** not LOCAL id
                if(ng.getArchetype().isInitRNGRequired()) {
                    groupEnv.add(Type::Void, "_rng", genGlobalRNGSkipAhead(groupEnv.getStream(), "id"));
                }

                ng.generateInit(*this, groupEnv, batchSize);
            }
        });
    env.getStream() << std::endl;

    env.getStream() << "// ------------------------------------------------------------------------" << std::endl;
    env.getStream() << "// Synapse groups" << std::endl;
    genParallelGroup<SynapseInitGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedSynapseInitGroups,
        [this](const SynapseGroupInternal &sg) { return padKernelSize(getNumInitThreads(sg), KernelInitialize); },
        [batchSize, this](EnvironmentExternalBase &env, SynapseInitGroupMerged &sg)
        {
            genSynapseVarInit(env, batchSize, sg, sg.getArchetype().isWUInitRNGRequired(), 
                              (sg.getArchetype().getMatrixType() & SynapseMatrixWeight::KERNEL), 
                              sg.getArchetype().getKernelSize().size());
        });
    env.getStream() << std::endl;

    env.getStream() << "// ------------------------------------------------------------------------" << std::endl;
    env.getStream() << "// Custom update groups" << std::endl;
    genParallelGroup<CustomUpdateInitGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedCustomUpdateInitGroups,
        [this](const CustomUpdateInternal &cg) { return padKernelSize(cg.getSize(), KernelInitialize); },
        [batchSize, this](EnvironmentExternalBase &env, CustomUpdateInitGroupMerged &cg)
        {
            env.getStream() << "// only do this for existing variables" << std::endl;
            env.print("if($(id) < $(size))");
            {
                CodeStream::Scope b(env.getStream());
                EnvironmentGroupMergedField<CustomUpdateInitGroupMerged> groupEnv(env, cg);

                // If this custom update requires an RNG for initialisation,
                // make copy of global phillox RNG and skip ahead by thread id
                // **NOTE** not LOCAL id
                if(cg.getArchetype().isInitRNGRequired()) {
                    groupEnv.add(Type::Void, "_rng", genGlobalRNGSkipAhead(groupEnv.getStream(), "id"));
                }

                cg.generateInit(*this, groupEnv, batchSize);
            }
        });
    env.getStream() << std::endl;

    env.getStream() << "// ------------------------------------------------------------------------" << std::endl;
    env.getStream() << "// Custom WU update groups" << std::endl;
    genParallelGroup<CustomWUUpdateInitGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedCustomWUUpdateInitGroups,
        [this](const CustomUpdateWUInternal &cg) { return padKernelSize(getNumInitThreads(cg), KernelInitialize); },
        [batchSize, this](EnvironmentExternalBase &env, CustomWUUpdateInitGroupMerged &cg)
        {
            const SynapseGroup *sg = cg.getArchetype().getSynapseGroup();
            genSynapseVarInit(env, batchSize, cg, cg.getArchetype().isInitRNGRequired(), 
                              (sg->getMatrixType() & SynapseMatrixWeight::KERNEL), sg->getKernelSize().size());
        });
    env.getStream() << std::endl;

    env.getStream() << "// ------------------------------------------------------------------------" << std::endl;
    env.getStream() << "// Custom connectivity presynaptic update groups" << std::endl;
    genParallelGroup<CustomConnectivityUpdatePreInitGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedCustomConnectivityUpdatePreInitGroups,
        [this](const CustomConnectivityUpdateInternal &cg) { return padKernelSize(cg.getSynapseGroup()->getSrcNeuronGroup()->getNumNeurons(), KernelInitialize); },
        [batchSize, this](EnvironmentExternalBase &env, CustomConnectivityUpdatePreInitGroupMerged &cg)
        {
            env.getStream() << "// only do this for existing variables" << std::endl;
            env.print("if($(id) < $(size))");
            {
                CodeStream::Scope b(env.getStream());
                EnvironmentGroupMergedField<CustomConnectivityUpdatePreInitGroupMerged> groupEnv(env, cg);

                // If population RNGs are initialised on device and this custom connectivity update 
                // required one, initialise single RNG using GLOBAL thread id for sequence
                if(isPopulationRNGInitialisedOnDevice() && Utils::isRNGRequired(cg.getArchetype().getRowUpdateCodeTokens())) {
                    // Add field for RNG
                    EnvironmentGroupMergedField<CustomConnectivityUpdatePreInitGroupMerged> rngInitEnv(groupEnv, cg);
                    rngInitEnv.addField(getPopulationRNGType().createPointer(), "_rng", "rng",
                                        [this](const auto &g, size_t) { return getDeviceVarPrefix() + "rng" + g.getName(); });

                    genPopulationRNGInit(rngInitEnv.getStream(), printSubs("$(_rng)[$(id)]", groupEnv), 
                                         "deviceRNGSeed", "id");
                }

                // If this custom update requires an RNG for initialisation,
                // make copy of global phillox RNG and skip ahead by thread id
                // **NOTE** not LOCAL id
                if(Utils::isRNGRequired(cg.getArchetype().getPreVarInitialisers())) {
                    groupEnv.add(Type::Void, "_rng", genGlobalRNGSkipAhead(groupEnv.getStream(), "id"));
                }

                cg.generateInit(*this, groupEnv, batchSize);
            }
        });
    env.getStream() << std::endl;

    env.getStream() << "// ------------------------------------------------------------------------" << std::endl;
    env.getStream() << "// Custom connectivity postsynaptic update groups" << std::endl;
    genParallelGroup<CustomConnectivityUpdatePostInitGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedCustomConnectivityUpdatePostInitGroups,
        [this](const CustomConnectivityUpdateInternal &cg) { return padKernelSize(cg.getSynapseGroup()->getTrgNeuronGroup()->getNumNeurons(), KernelInitialize); },
        [batchSize, this](EnvironmentExternalBase &env, CustomConnectivityUpdatePostInitGroupMerged &cg)
        {
            env.getStream() << "// only do this for existing variables" << std::endl;
            env.print("if($(id) < $(size))");
            {
                CodeStream::Scope b(env.getStream());
                EnvironmentGroupMergedField<CustomConnectivityUpdatePostInitGroupMerged> groupEnv(env, cg);

                // If population RNGs are initialised on device and this custom connectivity update 
                // required one, initialise single RNG using GLOBAL thread id for sequence
                if(isPopulationRNGInitialisedOnDevice() && Utils::isRNGRequired(cg.getArchetype().getRowUpdateCodeTokens())) {
                    // Add field for RNG
                    EnvironmentGroupMergedField<CustomConnectivityUpdatePostInitGroupMerged> rngInitEnv(groupEnv, cg);
                    rngInitEnv.addField(getPopulationRNGType().createPointer(), "_rng", "rng",
                                        [this](const auto &g, size_t) { return getDeviceVarPrefix() + "rng" + g.getName(); });

                    genPopulationRNGInit(rngInitEnv.getStream(), printSubs("$(_rng)[$(id)]", groupEnv), 
                                         "deviceRNGSeed", "id");
                }

                // If this custom update requires an RNG for initialisation,
                // make copy of global phillox RNG and skip ahead by thread id
                // **NOTE** not LOCAL id
                if(Utils::isRNGRequired(cg.getArchetype().getPostVarInitialisers())) {
                    groupEnv.add(Type::Void, "_rng", genGlobalRNGSkipAhead(groupEnv.getStream(), "id"));
                }

                cg.generateInit(*this, groupEnv, batchSize);
            }
        });
    env.getStream() << std::endl;

    env.getStream() << "// ------------------------------------------------------------------------" << std::endl;
    env.getStream() << "// Synapse groups with sparse connectivity" << std::endl;
    genParallelGroup<SynapseConnectivityInitGroupMerged>(
        env, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedSynapseConnectivityInitGroups,
        [this](const SynapseGroupInternal &sg) { return padKernelSize(getNumConnectivityInitThreads(sg), KernelInitialize); },
        [&modelMerged, this](EnvironmentExternalBase &env, SynapseConnectivityInitGroupMerged &sg)
        {
            EnvironmentGroupMergedField<SynapseConnectivityInitGroupMerged> groupEnv(env, sg);
            buildStandardEnvironment(groupEnv, modelMerged.getModel().getBatchSize());

            // If there is row-building code in this snippet
            const auto &connectInit = sg.getArchetype().getConnectivityInitialiser();
            if(!Utils::areTokensEmpty(connectInit.getRowBuildCodeTokens())) {
                groupEnv.getStream() << "// only do this for existing presynaptic neurons" << std::endl;
                groupEnv.print("if($(id) < $(num_pre))");

                // Configure substitutions
                groupEnv.add(Type::Uint32.addConst(), "id_pre", "$(id)");
                groupEnv.add(Type::Uint32.addConst(), "id_post_begin", "0");
                groupEnv.add(Type::Uint32.addConst(), "id_thread", "0");
                groupEnv.add(Type::Uint32.addConst(), "num_threads", "1");
            }
            // Otherwise
            else {
                assert(!Utils::areTokensEmpty(connectInit.getColBuildCodeTokens()));

                groupEnv.getStream() << "// only do this for existing postsynaptic neurons" << std::endl;
                groupEnv.print("if($(id) < $(num_post))");

                // Configure substitutions
                groupEnv.add(Type::Uint32.addConst(), "id_post", "$(id)");
                groupEnv.add(Type::Uint32.addConst(), "id_pre_begin", "0");
                groupEnv.add(Type::Uint32.addConst(), "id_thread", "0");
                groupEnv.add(Type::Uint32.addConst(), "num_threads", "1");
            }
            {
                CodeStream::Scope b(groupEnv.getStream());

                // Create new stream to generate addSynapse function which initializes all kernel variables
                std::ostringstream kernelInitStream;
                CodeStream kernelInit(kernelInitStream);

                // Use classic macro trick to turn block of initialization code into statement and 'eat' semicolon
                kernelInit << "do";
                {
                    CodeStream::Scope b(kernelInit);

                    // Calculate index in data structure of this synapse
                    if(sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                        if(!Utils::areTokensEmpty(connectInit.getRowBuildCodeTokens())) {
                            kernelInit << "const unsigned int idx = ($(id_pre) * $(_row_stride)) + $(_row_length)[$(id)];" << std::endl;
                        }
                        else {
                            kernelInit << "const unsigned int idx = (($(0)) * $(_row_stride))) + $(_row_length)[$(0)];" << std::endl;
                        }
                    }

                    // If there is a kernel
                    if(!sg.getArchetype().getKernelSize().empty()) {
                        assert(false);
                        /*Substitutions kernelInitSubs(&popSubs);

                        // Replace $(id_post) with first 'function' parameter as simulation code is
                        // going to be, in turn, substituted into procedural connectivity generation code
                        if(!snippet->getRowBuildCode().empty()) {
                            kernelInitSubs.addVarSubstitution("id_post", "$(0)");
                        }
                        else {
                            kernelInitSubs.addVarSubstitution("id_pre", "$(0)");
                        }

                        // Add index of synapse
                        if(sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                            kernelInitSubs.addVarSubstitution("id_syn", "idx");
                        }

                        // Replace kernel indices with the subsequent 'function' parameters
                        for(size_t i = 0; i < sg.getArchetype().getKernelSize().size(); i++) {
                            kernelInitSubs.addVarSubstitution("id_kernel_" + std::to_string(i), "$(" + std::to_string(i + 1) + ")");
                        }

                        // Call handler to initialize variables
                        sg.generateKernelInit(*this, kernelInit, modelMerged, kernelInitSubs);*/
                    }

                    // If matrix is sparse
                    if(sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                        // If there is row-building code in this snippet
                        if(!Utils::areTokensEmpty(connectInit.getRowBuildCodeTokens())) {
                            kernelInit << "$(_ind)[idx] = $(0);" << std::endl;
                            kernelInit << "$(_row_length)[$(id)]++;" << std::endl;
                        }
                        // Otherwise
                        else {
                            kernelInit << "$(_ind)[(($(0)) * $(_row_stride)) + " << getAtomic(Type::Uint32) << +"(&$(_row_length)[$(0)], 1)] = $(id_post);";
                        }
                    }
                    // Otherwise, if it's bitmask
                    else {
                        // Figure out required type for indexing into bitmask
                        const std::string indexType = areSixtyFourBitSynapseIndicesRequired(sg) ? "uint64_t" : "unsigned int";

                        // If there is row-building code in this snippet
                        if(!Utils::areTokensEmpty(connectInit.getRowBuildCodeTokens())) {
                            kernelInit << "const " << indexType << " rowStartGID = $(id) * (" << indexType << ")($_row_stride);" << std::endl;
                            kernelInit << getAtomic(Type::Uint32, AtomicOperation::OR) << "(&$(_gp)[(rowStartGID + ($(0))) / 32], 0x80000000 >> ((rowStartGID + ($(0))) & 31));" << std::endl;
                        }
                        // Otherwise
                        else {
                            kernelInit << "const " << indexType << " colStartGID = $(id);" << std::endl;
                            kernelInit << getAtomic(Type::Uint32, AtomicOperation::OR) << "(&$(_gp)[(colStartGID + (($(0)) * $(_row_stride))) / 32], 0x80000000 >> ((colStartGID + (($(0)) * $(_row_stride))) & 31));" << std::endl;
                        }
                    }
                }
                kernelInit << "while(false)";

                groupEnv.add(Type::ResolvedType::createFunction(Type::Void, {Type::Uint32}), "addSynapse", //1 + (unsigned int)sg.getArchetype().getKernelSize().size(),
                             kernelInitStream.str());

                // If this connectivity requires an RNG for initialisation,
                // make copy of global phillox RNG and skip ahead by thread id
                // **NOTE** not LOCAL id
                if(connectInit.isRNGRequired()) {
                    groupEnv.add(Type::Void, "_rng", genGlobalRNGSkipAhead(groupEnv.getStream(), "id"));
                }

                // If there is row-building code in this snippet
                if(!Utils::areTokensEmpty(connectInit.getRowBuildCodeTokens())) {
                    // If this is a sparse matrix, zero row length
                    if(sg.getArchetype().getMatrixType() & SynapseMatrixConnectivity::SPARSE) {
                        groupEnv.printLine("$(_row_length)[$(id)] = 0;");
                    }

                    // Call row-based connectivity handler
                    sg.generateSparseRowInit(*this, groupEnv);
                }
                // Otherwise, call column-based connectivity handler
                // **NOTE** in this case, row length gets zeroed by a memset call in backend
                else {
                    sg.generateSparseColumnInit(*this, groupEnv);
                }
            }
        });
    env.getStream() << std::endl;
}
//--------------------------------------------------------------------------
void BackendSIMT::genInitializeSparseKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged,
                                            size_t numInitializeThreads, BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const
{
    EnvironmentExternal envKernel(env);
    envKernel.add(Type::Void, "_sh_row_length", "shRowLength",
                  {envKernel.addInitialiser(getSharedPrefix() + "unsigned int shRowLength[" + std::to_string(getKernelBlockSize(KernelInitializeSparse)) + "];")});
   
    // Initialise weight update variables for synapse groups with sparse connectivity
    const unsigned int batchSize = modelMerged.getModel().getBatchSize();
    genParallelGroup<SynapseSparseInitGroupMerged>(
        envKernel, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedSynapseSparseInitGroups,
        [this](const SynapseGroupInternal &sg) { return padKernelSize(getNumConnectivityInitThreads(sg), KernelInitializeSparse); },
        [batchSize, numInitializeThreads, this](EnvironmentExternalBase &env, SynapseSparseInitGroupMerged &sg)
        {
            EnvironmentGroupMergedField<SynapseSparseInitGroupMerged> groupEnv(env, sg);
            buildStandardEnvironment(groupEnv, batchSize);

            // If this post synapse requires an RNG for initialisation,
            // make copy of global phillox RNG and skip ahead by thread id
            // **NOTE** not LOCAL id
            if(sg.getArchetype().isWUInitRNGRequired()) {
                groupEnv.add(Type::Void, "_rng", 
                             genGlobalRNGSkipAhead(groupEnv.getStream(), std::to_string(numInitializeThreads) + " + id"));
            }

            // Generate sparse synapse variable initialisation code
            genSparseSynapseVarInit<SynapseSparseInitGroupMerged>(
                groupEnv, batchSize, sg, sg.getArchetype().isWUVarInitRequired(), 
                [this](EnvironmentExternalBase &env, SynapseSparseInitGroupMerged &sg)
                {
                    // If postsynaptic learning is required
                    if(!sg.getArchetype().getWUModel()->getLearnPostCode().empty()) {
                        CodeStream::Scope b(env.getStream());

                        // Extract index of synapse's postsynaptic target
                        env.printLine("const unsigned int postIndex = $(_ind)[idx];");

                        // Atomically increment length of column of connectivity associated with this target
                        // **NOTE** this returns previous length i.e. where to insert new entry
                        env.printLine("const unsigned int colLocation = " + getAtomic(Type::Uint32) + "(&$(_col_length)[postIndex], 1);");

                        // From this calculate index into column-major matrix
                        env.printLine("const unsigned int colMajorIndex = (postIndex * $(_col_stride)) + colLocation;");

                        // Add remapping entry at this location poining back to row-major index
                        env.printLine("$(_remap)[colMajorIndex] = idx;");
                    }
                });
        });

    // Initialise weight update variables for synapse groups with sparse connectivity
    genParallelGroup<CustomWUUpdateSparseInitGroupMerged>(
        envKernel, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedCustomWUUpdateSparseInitGroups,
        [this](const CustomUpdateWUInternal &cg) { return padKernelSize(cg.getSynapseGroup()->getMaxConnections(), KernelInitializeSparse); },
        [batchSize, numInitializeThreads, this](EnvironmentExternalBase &env, CustomWUUpdateSparseInitGroupMerged &cg)
        {
            EnvironmentGroupMergedField<CustomWUUpdateSparseInitGroupMerged> groupEnv(env, cg);

            // If this custom update requires an RNG for initialisation,
            // make copy of global phillox RNG and skip ahead by thread id
            // **NOTE** not LOCAL id
            if(cg.getArchetype().isInitRNGRequired()) {
                groupEnv.add(Type::Void, "_rng", 
                             genGlobalRNGSkipAhead(groupEnv.getStream(), std::to_string(numInitializeThreads) + " + id"));
            }
            
            // Generate sparse synapse variable initialisation code
            genSparseSynapseVarInit<CustomWUUpdateSparseInitGroupMerged>(
                groupEnv, batchSize, cg, true,
                [](EnvironmentExternalBase&, CustomWUUpdateSparseInitGroupMerged&){});
        });

    // Initialise weight update variables for synapse groups with sparse connectivity
    genParallelGroup<CustomConnectivityUpdateSparseInitGroupMerged>(
        envKernel, modelMerged, memorySpaces, idStart, &ModelSpecMerged::genMergedCustomConnectivityUpdateSparseInitGroups,
        [this](const CustomConnectivityUpdateInternal &cg) { return padKernelSize(cg.getSynapseGroup()->getMaxConnections(), KernelInitializeSparse); },
        [batchSize, numInitializeThreads, this](EnvironmentExternalBase &env, CustomConnectivityUpdateSparseInitGroupMerged &cg)
        {
            EnvironmentGroupMergedField<CustomConnectivityUpdateSparseInitGroupMerged> groupEnv(env, cg);

            // If this custom update requires an RNG for initialisation,
            // make copy of global phillox RNG and skip ahead by thread id
            // **NOTE** not LOCAL id
            if(Utils::isRNGRequired(cg.getArchetype().getVarInitialisers())) {
                groupEnv.add(Type::Void, "_rng", 
                              genGlobalRNGSkipAhead(groupEnv.getStream(), std::to_string(numInitializeThreads) + " + id"));
            }
            
            // Generate sparse synapse variable initialisation code
            genSparseSynapseVarInit<CustomConnectivityUpdateSparseInitGroupMerged>(
                groupEnv, batchSize, cg, true,
                [](EnvironmentExternalBase&, CustomConnectivityUpdateSparseInitGroupMerged&){});
        });
}
//--------------------------------------------------------------------------
size_t BackendSIMT::padKernelSize(size_t size, Kernel kernel) const
{ 
    return padSize(size, getKernelBlockSize(kernel)); 
}
//--------------------------------------------------------------------------
void BackendSIMT::genEmitSpike(EnvironmentExternalBase &env, const std::string &suffix, bool recordingEnabled) const
{
    env.printLine("const unsigned int spk" + suffix + "_idx = " + getAtomic(Type::Uint32, AtomicOperation::ADD, AtomicMemSpace::SHARED) + "(&$(_sh_spk" + suffix + "_count), 1);");
    env.printLine("$(_sh_spk" + suffix + ")[spk" + suffix + "_idx] = $(id);");
    
    // If recording is enabled, set bit in recording word
    if(recordingEnabled) {
        if(m_KernelBlockSizes[KernelNeuronUpdate] == 32) {
            env.getStream() << getAtomic(Type::Uint32, AtomicOperation::OR, AtomicMemSpace::SHARED) << "(&shSpk" << suffix << "Record, 1 << " << getThreadID() << ");" << std::endl;
        }
        else {
            env.getStream() << getAtomic(Type::Uint32, AtomicOperation::OR, AtomicMemSpace::SHARED) << "(&shSpk" << suffix << "Record[" << getThreadID() << " / 32], 1 << (" << getThreadID() << " % 32));" << std::endl;
        }
    }
}
//--------------------------------------------------------------------------
void BackendSIMT::genRecordingSharedMemInit(CodeStream &os, const std::string &suffix) const
{
    if(m_KernelBlockSizes[KernelNeuronUpdate] == 32) {
        os << getSharedPrefix() << "uint32_t shSpk" << suffix << "Record;" << std::endl;
        os << "if (" << getThreadID() << " == 0)";
        {
            CodeStream::Scope b(os);
            os << "shSpk" << suffix << "Record = 0;" << std::endl;
        }
    }
    else {
        os << getSharedPrefix() << "uint32_t shSpk" << suffix << "Record[" << m_KernelBlockSizes[KernelNeuronUpdate] / 32 << "];" << std::endl;
        os << "if (" << getThreadID() << " < " << m_KernelBlockSizes[KernelNeuronUpdate] / 32 << ")";
        {
            CodeStream::Scope b(os);
            os << "shSpk" << suffix << "Record[" << getThreadID() << "] = 0;" << std::endl;
        }
    }
}
//--------------------------------------------------------------------------
void BackendSIMT::genSynapseVariableRowInit(EnvironmentExternalBase &env, HandlerEnv handler) const
{
    EnvironmentExternal varEnv(env);

    // **TODO** 64-bit id_syn
    varEnv.add(Type::Uint32.addConst(), "id_syn", "($(id_pre) * $(_row_stride)) + $(id)");
    handler(varEnv);
}
//--------------------------------------------------------------------------
const PresynapticUpdateStrategySIMT::Base *BackendSIMT::getPresynapticUpdateStrategy(const SynapseGroupInternal &sg,
                                                                                     const PreferencesBase &preferences)
{
    // Loop through presynaptic update strategies until we find one that is compatible with this synapse group
    // **NOTE** this is done backwards so that user-registered strategies get first priority
    for(auto s = s_PresynapticUpdateStrategies.rbegin(); s != s_PresynapticUpdateStrategies.rend(); ++s) {
        if((*s)->isCompatible(sg, preferences)) {
            return *s;
        }
    }

    throw std::runtime_error("Unable to find a suitable presynaptic update strategy for synapse group '" + sg.getName() + "'");
    return nullptr;
}
}   // namespace GeNN::CodeGenerator
