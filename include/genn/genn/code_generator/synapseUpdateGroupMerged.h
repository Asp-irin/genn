#pragma once

// GeNN includes
#include "synapseGroupInternal.h"

// GeNN code generator includes
#include "code_generator/groupMerged.h"

//----------------------------------------------------------------------------
// GeNN::CodeGenerator::SynapseGroupMergedBase
//----------------------------------------------------------------------------
namespace GeNN::CodeGenerator
{
class GENN_EXPORT SynapseGroupMergedBase : public GroupMerged<SynapseGroupInternal>
{
public:
    //------------------------------------------------------------------------
    // Public API
    //------------------------------------------------------------------------
    //! Should the weight update model parameter be implemented heterogeneously?
    bool isWUParamHeterogeneous(const std::string &paramName) const;

    //! Should the weight update model derived parameter be implemented heterogeneously?
    bool isWUDerivedParamHeterogeneous(const std::string &paramName) const;

    //! Should the weight update model variable initialization parameter be implemented heterogeneously?
    bool isVarInitParamHeterogeneous(const std::string &varName, const std::string &paramName) const;
    
    //! Should the weight update model variable initialization derived parameter be implemented heterogeneously?
    bool isVarInitDerivedParamHeterogeneous(const std::string &varName, const std::string &paramName) const;

    //! Should the sparse connectivity initialization parameter be implemented heterogeneously?
    bool isSparseConnectivityInitParamHeterogeneous(const std::string &paramName) const;

    //! Should the sparse connectivity initialization parameter be implemented heterogeneously?
    bool isSparseConnectivityInitDerivedParamHeterogeneous(const std::string &paramName) const;

    //! Should the Toeplitz connectivity initialization parameter be implemented heterogeneously?
    bool isToeplitzConnectivityInitParamHeterogeneous(const std::string &paramName) const;

    //! Should the Toeplitz connectivity initialization parameter be implemented heterogeneously?
    bool isToeplitzConnectivityInitDerivedParamHeterogeneous(const std::string &paramName) const;

    std::string getPreSlot(unsigned int batchSize) const;
    std::string getPostSlot(unsigned int batchSize) const;

    std::string getPreVarIndex(unsigned int batchSize, VarAccessDim varDims, const std::string &index) const
    {
        return getPreVarIndex(getArchetype().getSrcNeuronGroup()->isDelayRequired(), batchSize, varDims, index);
    }
    
    std::string getPostVarIndex(unsigned int batchSize, VarAccessDim varDims, const std::string &index) const
    {
        return getPostVarIndex(getArchetype().getTrgNeuronGroup()->isDelayRequired(), batchSize, varDims, index);
    }

    std::string getPreWUVarIndex(unsigned int batchSize, VarAccessDim varDims, const std::string &index) const
    {
        return getPreVarIndex(getArchetype().getDelaySteps() != 0, batchSize, varDims, index);
    }
    
    std::string getPostWUVarIndex(unsigned int batchSize, VarAccessDim varDims, const std::string &index) const
    {
        return getPostVarIndex(getArchetype().getBackPropDelaySteps() != 0, batchSize, varDims, index);
    }

    std::string getPostDenDelayIndex(unsigned int batchSize, const std::string &index, const std::string &offset) const;

    std::string getPreVarIndex(bool delay, unsigned int batchSize, VarAccessDim varDims, const std::string &index) const
    {
        return getPrePostVarIndex(delay, batchSize, varDims, index, "pre");
    }

    std::string getPostVarIndex(bool delay, unsigned int batchSize, VarAccessDim varDims, const std::string &index) const
    {
        return getPrePostVarIndex(delay, batchSize, varDims, index, "post");
    }

    std::string getPrePrevSpikeTimeIndex(bool delay, unsigned int batchSize, VarAccessDim varDims, const std::string &index) const;
    std::string getPostPrevSpikeTimeIndex(bool delay, unsigned int batchSize, VarAccessDim varDims, const std::string &index) const;
    
    std::string getPostISynIndex(unsigned int batchSize, const std::string &index) const
    {
        return ((batchSize == 1) ? "" : "$(_post_batch_offset) + ") + index;
    }

    std::string getPreISynIndex(unsigned int batchSize, const std::string &index) const
    {
        return ((batchSize == 1) ? "" : "$(pre_batch_offset) + ") + index;
    }

    std::string getSynVarIndex(unsigned int batchSize, VarAccessDim varDims, const std::string &index) const;
    std::string getKernelVarIndex(unsigned int batchSize, VarAccessDim varDims, const std::string &index) const;
    
    boost::uuids::detail::sha1::digest_type getHashDigest() const;

protected:
    using GroupMerged::GroupMerged;
    

private:
    //------------------------------------------------------------------------
    // Private methods
    //------------------------------------------------------------------------
    std::string getPrePostVarIndex(bool delay, unsigned int batchSize, VarAccessDim varDims,
                                   const std::string &index, const std::string &prefix) const;
};

//----------------------------------------------------------------------------
// GeNN::CodeGenerator::PresynapticUpdateGroupMerged
//----------------------------------------------------------------------------
class GENN_EXPORT PresynapticUpdateGroupMerged : public SynapseGroupMergedBase
{
public:
    using SynapseGroupMergedBase::SynapseGroupMergedBase;

    void generateRunner(const BackendBase &backend, CodeStream &definitions) const
    {
        generateRunnerBase(backend, definitions, name);
    }

    void generateSpikeEventThreshold(EnvironmentExternalBase &env, unsigned int batchSize);
    void generateSpikeEventUpdate(EnvironmentExternalBase &env, 
                                  unsigned int batchSize, double dt);
    void generateSpikeUpdate(EnvironmentExternalBase &env, 
                             unsigned int batchSize, double dt);
    void generateProceduralConnectivity(EnvironmentExternalBase &env);
    void generateToeplitzConnectivity(EnvironmentExternalBase &env,
                                      Transpiler::TypeChecker::StatementHandler forEachSynapseTypeCheckHandler,
                                      Transpiler::PrettyPrinter::StatementHandler forEachSynapsePrettyPrintHandler);

    //----------------------------------------------------------------------------
    // Static constants
    //----------------------------------------------------------------------------
    static const std::string name;
};

//----------------------------------------------------------------------------
// GeNN::CodeGenerator::PostsynapticUpdateGroupMerged
//----------------------------------------------------------------------------
class GENN_EXPORT PostsynapticUpdateGroupMerged : public SynapseGroupMergedBase
{
public:
    using SynapseGroupMergedBase::SynapseGroupMergedBase;

    void generateRunner(const BackendBase &backend, CodeStream &definitions) const
    {
        generateRunnerBase(backend, definitions, name);
    }

    void generateSynapseUpdate(EnvironmentExternalBase &env, 
                               unsigned int batchSize, double dt);
    
    //----------------------------------------------------------------------------
    // Static constants
    //----------------------------------------------------------------------------
    static const std::string name;
};

//----------------------------------------------------------------------------
// GeNN::CodeGenerator::SynapseDynamicsGroupMerged
//----------------------------------------------------------------------------
class GENN_EXPORT SynapseDynamicsGroupMerged : public SynapseGroupMergedBase
{
public:
    using SynapseGroupMergedBase::SynapseGroupMergedBase;

    void generateRunner(const BackendBase &backend, CodeStream &definitions) const
    {
        generateRunnerBase(backend, definitions, name);
    }

    void generateSynapseUpdate(EnvironmentExternalBase &env, 
                               unsigned int batchSize, double dt);

    //----------------------------------------------------------------------------
    // Static constants
    //----------------------------------------------------------------------------
    static const std::string name;
};

//----------------------------------------------------------------------------
// GeNN::CodeGenerator::SynapseDendriticDelayUpdateGroupMerged
//----------------------------------------------------------------------------
class GENN_EXPORT SynapseDendriticDelayUpdateGroupMerged : public GroupMerged<SynapseGroupInternal>
{
public:
    using GroupMerged::GroupMerged;

    //------------------------------------------------------------------------
    // Public API
    //------------------------------------------------------------------------
    void generateRunner(const BackendBase &backend, CodeStream &definitions) const
    {
        generateRunnerBase(backend, definitions, name);
    }

    //----------------------------------------------------------------------------
    // Static constants
    //----------------------------------------------------------------------------
    static const std::string name;
};
}   // namespace GeNN::CodeGenerator
