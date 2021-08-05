#pragma once

// Standard C++ includes
#include <array>
#include <numeric>
#include <unordered_set>

// GeNN includes
#include "gennExport.h"
#include "varAccess.h"

// GeNN code generator includes
#include "code_generator/backendBase.h"
#include "code_generator/codeStream.h"
#include "code_generator/presynapticUpdateStrategySIMT.h"
#include "code_generator/substitutions.h"

//--------------------------------------------------------------------------
// CodeGenerator::Kernel
//--------------------------------------------------------------------------
namespace CodeGenerator
{
//! Kernels generated by SIMT backends
enum Kernel
{
    KernelNeuronUpdate,
    KernelPresynapticUpdate,
    KernelPostsynapticUpdate,
    KernelSynapseDynamicsUpdate,
    KernelInitialize,
    KernelInitializeSparse,
    KernelNeuronSpikeQueueUpdate,
    KernelNeuronPrevSpikeTimeUpdate,
    KernelSynapseDendriticDelayUpdate,
    KernelCustomUpdate,
    KernelCustomTransposeUpdate,
    KernelMax
};

//--------------------------------------------------------------------------
// Type definitions
//--------------------------------------------------------------------------
//! Array of block sizes for each kernel
using KernelBlockSize = std::array<size_t, KernelMax>;

//--------------------------------------------------------------------------
// CodeGenerator::BackendSIMT
//--------------------------------------------------------------------------
//! Base class for Single Instruction Multiple Thread style backends
/*! CUDA terminology is used throughout i.e. thread blocks and shared memory */
class GENN_EXPORT BackendSIMT : public BackendBase
{
public:
    BackendSIMT(const KernelBlockSize &kernelBlockSizes, const PreferencesBase &preferences, 
                const std::string &scalarType)
    :   BackendBase(scalarType, preferences), m_KernelBlockSizes(kernelBlockSizes)
    {}

    //------------------------------------------------------------------------
    // Enumerations
    //------------------------------------------------------------------------
    //! What atomic operation is required
    enum class AtomicOperation
    {
        ADD,
        OR,
    };

    //! What memory space atomic operation is required
    enum class AtomicMemSpace
    {
        GLOBAL,
        SHARED,
    };

    //------------------------------------------------------------------------
    // Declared virtuals
    //------------------------------------------------------------------------
    //! On some older devices, shared memory atomics are actually slower than global memory atomics so should be avoided
    virtual bool areSharedMemAtomicsSlow() const = 0;

    //! Get the prefix to use for shared memory variables
    virtual std::string getSharedPrefix() const = 0;

    //! Get the ID of the current thread within the threadblock
    virtual std::string getThreadID(unsigned int axis = 0) const = 0;

    //! Get the ID of the current thread block
    virtual std::string getBlockID(unsigned int axis = 0) const = 0;

    //! Get the name of the count-leading-zeros function
    virtual std::string getCLZ() const = 0;

    //! Get name of atomic operation
    virtual std::string getAtomic(const std::string &type, AtomicOperation op = AtomicOperation::ADD, 
                                  AtomicMemSpace memSpace = AtomicMemSpace::GLOBAL) const = 0;

    //! Generate a shared memory barrier
    virtual void genSharedMemBarrier(CodeStream &os) const = 0;

    //! For SIMT backends which initialize RNGs on device, initialize population RNG with specified seed and sequence
    virtual void genPopulationRNGInit(CodeStream &os, const std::string &globalRNG, const std::string &seed, const std::string &sequence) const = 0;

    //! Generate a preamble to add substitution name for population RNG
    virtual void genPopulationRNGPreamble(CodeStream &os, Substitutions &subs, const std::string &globalRNG, const std::string &name = "rng") const = 0;
    
    //! If required, generate a postamble for population RNG
    /*! For example, in OpenCL, this is used to write local RNG state back to global memory*/
    virtual void genPopulationRNGPostamble(CodeStream &os, const std::string &globalRNG) const = 0;

    //! Generate code to skip ahead local copy of global RNG
    virtual void genGlobalRNGSkipAhead(CodeStream &os, Substitutions &subs, const std::string &sequence, const std::string &name = "rng") const = 0;

    //------------------------------------------------------------------------
    // BackendBase virtuals
    //------------------------------------------------------------------------
    //! Gets the stride used to access synaptic matrix rows, taking into account sparse data structure, padding etc
    virtual size_t getSynapticMatrixRowStride(const SynapseGroupInternal &sg) const final;

    //! When backends require separate 'device' and 'host' versions of variables, they are identified with a prefix.
    //! This function returns the device prefix so it can be used in otherwise platform-independent code.
    virtual std::string getDeviceVarPrefix() const final { return getPreferences().automaticCopy ? "" : "d_"; }

    virtual void genPopVariableInit(CodeStream &os, const Substitutions &kernelSubs, Handler handler) const final;
    virtual void genVariableInit(CodeStream &os, const std::string &count, const std::string &indexVarName,
                                 const Substitutions &kernelSubs, Handler handler) const final;
    virtual void genSparseSynapseVariableRowInit(CodeStream &os, const Substitutions &kernelSubs, Handler handler) const final
    {
        genSynapseVariableRowInit(os, kernelSubs, handler);
    }

    virtual void genDenseSynapseVariableRowInit(CodeStream &os, const Substitutions &kernelSubs, Handler handler) const final
    {
        genSynapseVariableRowInit(os, kernelSubs, handler);
    }


    //! Should 'scalar' variables be implemented on device or can host variables be used directly?
    virtual bool isDeviceScalarRequired() const final { return true; }

    virtual bool isGlobalHostRNGRequired(const ModelSpecMerged &modelMerged) const final;
    virtual bool isGlobalDeviceRNGRequired(const ModelSpecMerged &modelMerged) const final;
    virtual bool isPopulationRNGRequired() const final { return true; }

    virtual bool isSynRemapRequired(const SynapseGroupInternal &sg) const final;
    virtual bool isPostsynapticRemapRequired() const final { return true; }

    //------------------------------------------------------------------------
    // Public API
    //------------------------------------------------------------------------
    //! Get total number of RNG streams potentially used to initialise model
    /*! **NOTE** because RNG supports 2^64 streams, we are overly conservative */
    size_t getNumInitialisationRNGStreams(const ModelSpecMerged & modelMerged) const;

    size_t getKernelBlockSize(Kernel kernel) const { return m_KernelBlockSizes.at(kernel); }

    size_t getPaddedNumCustomUpdateWUThreads(const CustomUpdateWUInternal &cg, unsigned int batchSize) const;
    size_t getPaddedNumCustomUpdateTransposeWUThreads(const CustomUpdateWUInternal &cg, unsigned int batchSize) const;
    
    //--------------------------------------------------------------------------
    // Static API
    //--------------------------------------------------------------------------
    static size_t getNumPresynapticUpdateThreads(const SynapseGroupInternal &sg, const PreferencesBase &preferences);
    static size_t getNumPostsynapticUpdateThreads(const SynapseGroupInternal &sg);
    static size_t getNumSynapseDynamicsThreads(const SynapseGroupInternal &sg);
    static size_t getNumConnectivityInitThreads(const SynapseGroupInternal &sg);

    //! Register a new presynaptic update strategy
    /*! This function should be called with strategies in ascending order of preference */
    static void addPresynapticUpdateStrategy(PresynapticUpdateStrategySIMT::Base *strategy);

    //--------------------------------------------------------------------------
    // Constants
    //--------------------------------------------------------------------------
    static const char *KernelNames[KernelMax];

protected:
    //------------------------------------------------------------------------
    // Protected API
    //------------------------------------------------------------------------
    void genNeuronPrevSpikeTimeUpdateKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged, size_t &idStart) const;
    void genNeuronSpikeQueueUpdateKernel(CodeStream &os, const ModelSpecMerged &modelMerged, size_t &idStart) const;

    void genNeuronUpdateKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                               NeuronGroupSimHandler simHandler, NeuronUpdateGroupMergedHandler wuVarUpdateHandler, size_t &idStart) const;

    void genSynapseDendriticDelayUpdateKernel(CodeStream &os, const ModelSpecMerged &modelMerged, size_t &idStart) const;
    void genPresynapticUpdateKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                                    PresynapticUpdateGroupMergedHandler wumThreshHandler, PresynapticUpdateGroupMergedHandler wumSimHandler,
                                    PresynapticUpdateGroupMergedHandler wumEventHandler, PresynapticUpdateGroupMergedHandler wumProceduralConnectHandler, size_t &idStart) const;
    void genPostsynapticUpdateKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                                     PostsynapticUpdateGroupMergedHandler postLearnHandler, size_t &idStart) const;
    void genSynapseDynamicsKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                                  SynapseDynamicsGroupMergedHandler synapseDynamicsHandler, size_t &idStart) const;

    void genCustomUpdateKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                               const std::string &updateGroup, CustomUpdateGroupMergedHandler &customUpdateHandler, size_t &idStart) const;

    void genCustomUpdateWUKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                                 const std::string &updateGroup, CustomUpdateWUGroupMergedHandler &customUpdateWUHandler, size_t &idStart) const;
    
    void genCustomTransposeUpdateWUKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                                          const std::string &updateGroup, CustomUpdateTransposeWUGroupMergedHandler &customWUTransposeUpdateHandler, size_t &idStart) const;

    void genInitializeKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                             NeuronInitGroupMergedHandler neuronInitHandler, CustomUpdateInitGroupMergedHandler cuHandler, 
                             CustomWUUpdateDenseInitGroupMergedHandler cuDenseHandler, SynapseDenseInitGroupMergedHandler synapseDenseInitHandler, 
                             SynapseConnectivityInitMergedGroupHandler sgSparseRowConnectHandler,  SynapseConnectivityInitMergedGroupHandler sgSparseColConnectHandler, 
                             SynapseConnectivityInitMergedGroupHandler sgKernelInitHandler, size_t &idStart) const;
   
    void genInitializeSparseKernel(CodeStream &os, const Substitutions &kernelSubs, const ModelSpecMerged &modelMerged,
                                   SynapseSparseInitGroupMergedHandler synapseSparseInitHandler, CustomWUUpdateSparseInitGroupMergedHandler cuSparseHandler, 
                                   size_t numInitializeThreads, size_t &idStart) const;

    //! Adds a type - both to backend base's list of sized types but also to device types set
    void addDeviceType(const std::string &type, size_t size, const std::string &maxValue = "");

    //! Is type a a device only type?
    bool isDeviceType(const std::string &type) const;
    
    //! Helper wrapper around padSize to pad size to a kernel size
    size_t padKernelSize(size_t size, Kernel kernel) const;

    //! Get kernel block size
    const KernelBlockSize &getKernelBlockSize() const { return m_KernelBlockSizes; }

private:
    //--------------------------------------------------------------------------
    // ReductionTarget
    //--------------------------------------------------------------------------
    //! Simple struct to hold reduction targets
    struct ReductionTarget
    {
        ReductionTarget(const std::string &n, const std::string &t, VarAccessMode a)
        : name(n), type(t), access(a)
        {
        }

        const std::string name;
        const std::string type;
        const VarAccessMode access;
    };


    //--------------------------------------------------------------------------
    // Type definitions
    //--------------------------------------------------------------------------
    template<typename T>
    using GetPaddedGroupSizeFunc = std::function<size_t(const T &)>;

    //--------------------------------------------------------------------------
    // Private methods
    //--------------------------------------------------------------------------
    template<typename T, typename S, typename F>
    void genParallelGroup(CodeStream &os, const Substitutions &kernelSubs, const std::vector<T> &groups, size_t &idStart,
                          S getPaddedSizeFunc, F filter, GroupHandler<T> handler) const
    {
        // Loop through groups
        for(const auto &gMerge : groups) {
            if(filter(gMerge)) {
                // Sum padded sizes of each group within merged group
                const size_t paddedSize = std::accumulate(
                    gMerge.getGroups().cbegin(), gMerge.getGroups().cend(), size_t{0},
                    [gMerge, getPaddedSizeFunc](size_t acc, std::reference_wrapper<const typename T::GroupInternal> g)
                    {
                        return (acc + getPaddedSizeFunc(g.get()));
                    });

                os << "// merged" << gMerge.getIndex() << std::endl;

                // If this is the first  group
                if(idStart == 0) {
                    os << "if(id < " << paddedSize << ")";
                }
                else {
                    os << "if(id >= " << idStart << " && id < " << idStart + paddedSize << ")";
                }
                {
                    CodeStream::Scope b(os);
                    Substitutions popSubs(&kernelSubs);

                    if(gMerge.getGroups().size() == 1) {
                        os << getPointerPrefix() << "struct Merged" << T::name << "Group" << gMerge.getIndex() << " *group";
                        os << " = &d_merged" << T::name << "Group" << gMerge.getIndex() << "[0]; " << std::endl;
                        os << "const unsigned int lid = id - " << idStart << ";" << std::endl;

                        // Use the starting thread ID of the whole merged group as group_start_id
                        popSubs.addVarSubstitution("group_start_id", std::to_string(idStart));
                    }
                    else {
                        // Perform bisect operation to get index of merged struct
                        os << "unsigned int lo = 0;" << std::endl;
                        os << "unsigned int hi = " << gMerge.getGroups().size() << ";" << std::endl;
                        os << "while(lo < hi)" << std::endl;
                        {
                            CodeStream::Scope b(os);
                            os << "const unsigned int mid = (lo + hi) / 2;" << std::endl;

                            os << "if(id < d_merged" << T::name << "GroupStartID" << gMerge.getIndex() << "[mid])";
                            {
                                CodeStream::Scope b(os);
                                os << "hi = mid;" << std::endl;
                            }
                            os << "else";
                            {
                                CodeStream::Scope b(os);
                                os << "lo = mid + 1;" << std::endl;
                            }
                        }

                        // Use this to get reference to merged group structure
                        os << getPointerPrefix() << "struct Merged" << T::name << "Group" << gMerge.getIndex() << " *group";
                        os << " = &d_merged" << T::name << "Group" << gMerge.getIndex() << "[lo - 1]; " << std::endl;

                        // Get group start thread ID and use as group_start_id
                        os << "const unsigned int groupStartID = d_merged" << T::name << "GroupStartID" << gMerge.getIndex() << "[lo - 1];" << std::endl;
                        popSubs.addVarSubstitution("group_start_id", "groupStartID");

                        // Use this to calculate local id within group
                        os << "const unsigned int lid = id - groupStartID;" << std::endl;
                    }
                    popSubs.addVarSubstitution("id", "lid");

                    handler(os, gMerge, popSubs);

                    idStart += paddedSize;
                }
            }
        }
    }

    
    template<typename G>
    std::vector<ReductionTarget> genInitReductionTargets(CodeStream &os, const G &cg) const
    {
        // Loop through variables
        std::vector<ReductionTarget> reductionTargets;
        const auto *cm = cg.getArchetype().getCustomUpdateModel();
        for(const auto &v : cm->getVars()) {
            // If variable is a reduction target, define variable initialised to correct initial value for reduction
            // **NOTE** by not initialising this, compilers should emit a warning if user code doesn't set it to something
            if(v.access & VarAccessModeAttribute::REDUCE) {
                os << v.type << " lr" << v.name << " = " << getReductionInitialValue(*this, getVarAccessMode(v.access), v.type) << ";" << std::endl;
                reductionTargets.emplace_back(v.name, v.type, getVarAccessMode(v.access));
            }
        }

        // Loop through variable references
        for(const auto &v : cm->getVarRefs()) {
            // If variable reference is a reduction target, define variable initialised to correct initial value for reduction
            // **NOTE** by not initialising this, compilers should emit a warning if user code doesn't set it to something
            if(v.access & VarAccessModeAttribute::REDUCE) {
                os << v.type << " lr" << v.name << " = " << getReductionInitialValue(*this, v.access, v.type) << ";" << std::endl;
                reductionTargets.emplace_back(v.name, v.type, v.access);
            }
        }
        return reductionTargets;
    }

    template<typename T, typename S>
    void genParallelGroup(CodeStream &os, const Substitutions &kernelSubs, const std::vector<T> &groups, size_t &idStart,
                          S getPaddedSizeFunc, GroupHandler<T> handler) const
    {
        genParallelGroup(os, kernelSubs, groups, idStart, getPaddedSizeFunc,
                         [](const T &) { return true; }, handler);
    }

    void genEmitSpike(CodeStream &os, const Substitutions &subs, const std::string &suffix, bool recordingEnabled) const;

    void genRecordingSharedMemInit(CodeStream &os, const std::string &suffix) const;

    void genSynapseVariableRowInit(CodeStream &os, const Substitutions &kernelSubs, Handler handler) const;

    // Get appropriate presynaptic update strategy to use for this synapse group
    const PresynapticUpdateStrategySIMT::Base *getPresynapticUpdateStrategy(const SynapseGroupInternal &sg) const
    {
        return getPresynapticUpdateStrategy(sg, getPreferences());
    }

    //--------------------------------------------------------------------------
    // Private static methods
    //--------------------------------------------------------------------------
    // Get appropriate presynaptic update strategy to use for this synapse group
    static const PresynapticUpdateStrategySIMT::Base *getPresynapticUpdateStrategy(const SynapseGroupInternal &sg,
                                                                                   const PreferencesBase &preferences);

    //--------------------------------------------------------------------------
    // Members
    //--------------------------------------------------------------------------
    const KernelBlockSize m_KernelBlockSizes;

    //! Types that are only supported on device i.e. should never be exposed to user code
    std::unordered_set<std::string> m_DeviceTypes;

    //--------------------------------------------------------------------------
    // Static members
    //--------------------------------------------------------------------------
    static std::vector<PresynapticUpdateStrategySIMT::Base *> s_PresynapticUpdateStrategies;
};

}   // namespace CodeGenerator
