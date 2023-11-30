#pragma once

// Standard includes
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// GeNN includes
#include "gennExport.h"
#include "neuronModels.h"
#include "varLocation.h"

// Forward declarations
namespace GeNN
{
class CurrentSourceInternal;
class SynapseGroupInternal;
}

//------------------------------------------------------------------------
// GeNN::NeuronGroup
//------------------------------------------------------------------------
namespace GeNN
{
class GENN_EXPORT NeuronGroup
{
public:
    //------------------------------------------------------------------------
    // SpikeEventThreshold
    //------------------------------------------------------------------------
    //! Structure used for storing spike event data
    struct SpikeEventThreshold
    {
        SpikeEventThreshold(const std::string &e, const std::string &s, bool egp, SynapseGroupInternal *sg)
            : eventThresholdCode(e), supportCode(s), synapseStateInThresholdCode(egp), synapseGroup(sg)
        {
        }

        const std::string eventThresholdCode;
        const std::string supportCode;
        const bool synapseStateInThresholdCode;
        SynapseGroupInternal *synapseGroup;

        //! Less than operator (used for std::set::insert), lexicographically compares all three struct
        //! members - meaning that event thresholds featuring extra global parameters or presynaptic
        //! state variables from different synapse groups will not get combined together in neuron update
        bool operator < (const SpikeEventThreshold &other) const
        {
            if (synapseStateInThresholdCode) {
                return (std::tie(eventThresholdCode, supportCode, synapseGroup) 
                        < std::tie(other.eventThresholdCode, other.supportCode, other.synapseGroup));
            }
            else {
                return (std::tie(eventThresholdCode, supportCode) 
                        < std::tie(other.eventThresholdCode, other.supportCode));
            }
        }

        //! Equality operator (used for set::set equality used when testing neuron groups mergability),
        //! Compares only the two code strings as neuron groups with threshold conditions 
        //! featuring extra global parameters from different synapse groups can still be merged
        bool operator == (const SpikeEventThreshold &other) const
        {
            return ((eventThresholdCode == other.eventThresholdCode)
                    && (supportCode == other.supportCode));
        }
    };

    NeuronGroup(const NeuronGroup&) = delete;
    NeuronGroup() = delete;

    //------------------------------------------------------------------------
    // Public methods
    //------------------------------------------------------------------------
    //! Set location of this neuron group's output spikes
    /*! This is ignored for simulations on hardware with a single memory space */
    void setSpikeLocation(VarLocation loc) { m_SpikeLocation = loc; }

    //! Set location of this neuron group's output spike times
    /*! This is ignored for simulations on hardware with a single memory space */
    void setSpikeTimeLocation(VarLocation loc) { m_SpikeTimeLocation = loc; }
    
    //! Set location of this neuron group's previous output spike times
    /*! This is ignored for simulations on hardware with a single memory space */
    void setPrevSpikeTimeLocation(VarLocation loc) { m_PrevSpikeTimeLocation = loc; }

    //! Set variable location of neuron model state variable
    /*! This is ignored for simulations on hardware with a single memory space */
    void setVarLocation(const std::string &varName, VarLocation loc);

    //! Set location of neuron model extra global parameter
    /*! This is ignored for simulations on hardware with a single memory space. */
    void setExtraGlobalParamLocation(const std::string &paramName, VarLocation loc);

    //! Set whether parameter is dynamic or not i.e. it can be changed at runtime
    void setParamDynamic(const std::string &paramName, bool dynamic = true);

    //! Enables and disable spike recording for this population
    void setSpikeRecordingEnabled(bool enabled) { m_SpikeRecordingEnabled = enabled; }
    
    //! Enables and disable spike event recording for this population
    void setSpikeEventRecordingEnabled(bool enabled) { m_SpikeEventRecordingEnabled = enabled; }

    //------------------------------------------------------------------------
    // Public const methods
    //------------------------------------------------------------------------
    const std::string &getName() const{ return m_Name; }

    //! Gets number of neurons in group
    unsigned int getNumNeurons() const{ return m_NumNeurons; }

    //! Gets the neuron model used by this group
    const NeuronModels::Base *getNeuronModel() const{ return m_NeuronModel; }

    const std::unordered_map<std::string, Type::NumericValue> &getParams() const{ return m_Params; }
    const std::unordered_map<std::string, InitVarSnippet::Init> &getVarInitialisers() const{ return m_VarInitialisers; }

    bool isSpikeTimeRequired() const;
    bool isPrevSpikeTimeRequired() const;
    bool isSpikeEventTimeRequired() const;
    bool isPrevSpikeEventTimeRequired() const;
    bool isTrueSpikeRequired() const;
    bool isSpikeEventRequired() const;

    unsigned int getNumDelaySlots() const{ return m_NumDelaySlots; }
    bool isDelayRequired() const{ return (m_NumDelaySlots > 1); }
    bool isZeroCopyEnabled() const;

    //! Get location of this neuron group's output spikes
    VarLocation getSpikeLocation() const{ return m_SpikeLocation; }

    //! Get location of this neuron group's output spike times
    VarLocation getSpikeTimeLocation() const{ return m_SpikeTimeLocation; }

    //! Get location of this neuron group's previous output spike times
    VarLocation getPrevSpikeTimeLocation() const { return m_PrevSpikeTimeLocation; }

    //! Get location of neuron model state variable by name
    VarLocation getVarLocation(const std::string &varName) const { return m_VarLocation.get(varName); }

    //! Get location of neuron model extra global parameter by name
    VarLocation getExtraGlobalParamLocation(const std::string &paramName) const { return m_ExtraGlobalParamLocation.get(paramName); }

    //! Is parameter dynamic i.e. it can be changed at runtime
    bool isParamDynamic(const std::string &paramName) const{ return m_DynamicParams.get(paramName); }

    //! Is spike recording enabled for this population?
    bool isSpikeRecordingEnabled() const { return m_SpikeRecordingEnabled; }

    //! Is spike event recording enabled for this population?
    bool isSpikeEventRecordingEnabled() const { return m_SpikeEventRecordingEnabled; }

protected:
    NeuronGroup(const std::string &name, int numNeurons, const NeuronModels::Base *neuronModel,
                const std::unordered_map<std::string, Type::NumericValue> &params, const std::unordered_map<std::string, InitVarSnippet::Init> &varInitialisers,
                VarLocation defaultVarLocation, VarLocation defaultExtraGlobalParamLocation);

    //------------------------------------------------------------------------
    // Protected methods
    //------------------------------------------------------------------------
    //! Checks delay slots currently provided by the neuron group against a required delay and extends if required
    void checkNumDelaySlots(unsigned int requiredDelay);
    
    // Set a variable as requiring queueing
    void setVarQueueRequired(const std::string &varName){ m_VarQueueRequired.insert(varName); }

    void addInSyn(SynapseGroupInternal *synapseGroup){ m_InSyn.push_back(synapseGroup); }
    void addOutSyn(SynapseGroupInternal *synapseGroup){ m_OutSyn.push_back(synapseGroup); }

    void finalise(double dt);

    //! Fuse incoming postsynaptic models
    void fusePrePostSynapses(bool fusePSM, bool fusePrePostWUM);

    //! add input current source
    void injectCurrent(CurrentSourceInternal *source);

    //------------------------------------------------------------------------
    // Protected const methods
    //------------------------------------------------------------------------
    //! Gets pointers to all synapse groups which provide input to this neuron group
    const std::vector<SynapseGroupInternal*> &getInSyn() const{ return m_InSyn; }
    const std::vector<SynapseGroupInternal*> &getFusedPSMInSyn() const{ return m_FusedPSMInSyn; }
    const std::vector<SynapseGroupInternal *> &getFusedWUPostInSyn() const { return m_FusedWUPostInSyn; }

    //! Gets pointers to all synapse groups emanating from this neuron group
    const std::vector<SynapseGroupInternal*> &getOutSyn() const{ return m_OutSyn; }
    const std::vector<SynapseGroupInternal *> &getFusedWUPreOutSyn() const { return m_FusedWUPreOutSyn; }
    const std::vector<SynapseGroupInternal *> &getFusedPreOutputOutSyn() const { return m_FusedPreOutputOutSyn; }

    const std::vector<SynapseGroupInternal*> &getFusedSpike() const{ return m_FusedSpike; }
    const std::vector<SynapseGroupInternal*> &getFusedSpikeEvent() const{ return m_FusedSpikeEvent; }
    
    //! Does this neuron group require an RNG to simulate?
    bool isSimRNGRequired() const;

    //! Does this neuron group require an RNG for it's init code?
    bool isInitRNGRequired() const;

    //! Does this neuron group require any sort of recording?
    bool isRecordingEnabled() const;

    //! Does this neuron group require any variables initializing?
    /*! Because it occurs in the same kernel, this includes current source variables;
        postsynaptic model variables and postsynaptic weight update variables 
        from incoming synapse groups; and presynaptic weight update variables from outgoing synapse groups */
    bool isVarInitRequired() const;

    //! Gets pointers to all current sources which provide input to this neuron group
    const std::vector<CurrentSourceInternal*> &getCurrentSources() const { return m_CurrentSourceGroups; }

    const std::unordered_map<std::string, Type::NumericValue> &getDerivedParams() const{ return m_DerivedParams; }

    const std::set<SpikeEventThreshold> &getSpikeEventCondition() const{ return m_SpikeEventCondition; }

    //! Helper to get vector of incoming synapse groups which have postsynaptic update code
    std::vector<SynapseGroupInternal*> getFusedInSynWithPostCode() const;

    //! Helper to get vector of outgoing synapse groups which have presynaptic update code
    std::vector<SynapseGroupInternal*> getFusedOutSynWithPreCode() const;

    //! Helper to get vector of incoming synapse groups which have postsynaptic variables
    std::vector<SynapseGroupInternal *> getFusedInSynWithPostVars() const;

    //! Helper to get vector of outgoing synapse groups which have presynaptic variables
    std::vector<SynapseGroupInternal *> getFusedOutSynWithPreVars() const;

    //! Tokens produced by scanner from simc ode
    const std::vector<Transpiler::Token> &getSimCodeTokens() const { return m_SimCodeTokens; }

    //! Tokens produced by scanner from threshold condition code
    const std::vector<Transpiler::Token> &getThresholdConditionCodeTokens() const { return m_ThresholdConditionCodeTokens; }
    
    //! Tokens produced by scanner from reset code
    const std::vector<Transpiler::Token> &getResetCodeTokens() const { return m_ResetCodeTokens; }

    bool isVarQueueRequired(const std::string &var) const;

    //! Updates hash with neuron group
    /*! NOTE: this can only be called after model is finalized */
    boost::uuids::detail::sha1::digest_type getHashDigest() const;

    //! Updates hash with neuron group initialisation
    /*! NOTE: this can only be called after model is finalized */
    boost::uuids::detail::sha1::digest_type getInitHashDigest() const;

    boost::uuids::detail::sha1::digest_type getSpikeQueueUpdateHashDigest() const;

    boost::uuids::detail::sha1::digest_type getPrevSpikeTimeUpdateHashDigest() const;

    boost::uuids::detail::sha1::digest_type getVarLocationHashDigest() const;

private:
    //------------------------------------------------------------------------
    // Members
    //------------------------------------------------------------------------
    const std::string m_Name;

    const unsigned int m_NumNeurons;

    const NeuronModels::Base *m_NeuronModel;
    const std::unordered_map<std::string, Type::NumericValue> m_Params;
    std::unordered_map<std::string, Type::NumericValue> m_DerivedParams;
    std::unordered_map<std::string, InitVarSnippet::Init> m_VarInitialisers;
    std::vector<SynapseGroupInternal*> m_InSyn;
    std::vector<SynapseGroupInternal*> m_OutSyn;
    std::vector<SynapseGroupInternal*> m_FusedPSMInSyn;
    std::vector<SynapseGroupInternal*> m_FusedSpike;
    std::vector<SynapseGroupInternal*> m_FusedSpikeEvent;
    std::vector<SynapseGroupInternal*> m_FusedWUPostInSyn;
    std::vector<SynapseGroupInternal*> m_FusedWUPreOutSyn;
    std::vector<SynapseGroupInternal*> m_FusedPreOutputOutSyn;
    std::set<SpikeEventThreshold> m_SpikeEventCondition;
    unsigned int m_NumDelaySlots;
    std::vector<CurrentSourceInternal*> m_CurrentSourceGroups;

    //! Set of names of variable requiring queueing
    std::unordered_set<std::string> m_VarQueueRequired;

    //! Location of spikes from neuron group
    VarLocation m_SpikeLocation;

    //! Location of spike times from neuron group
    VarLocation m_SpikeTimeLocation;

    //! Location of previous spike times
    VarLocation m_PrevSpikeTimeLocation;

    //! Location of individual state variables
    LocationContainer m_VarLocation;

    //! Location of extra global parameters
    LocationContainer m_ExtraGlobalParamLocation;

    //! Data structure tracking whether parameters are dynamic or not
    Snippet::DynamicParameterContainer m_DynamicParams;

    //! Tokens produced by scanner from simc ode
    std::vector<Transpiler::Token> m_SimCodeTokens;

    //! Tokens produced by scanner from threshold condition code
    std::vector<Transpiler::Token> m_ThresholdConditionCodeTokens;
    
    //! Tokens produced by scanner from reset code
    std::vector<Transpiler::Token> m_ResetCodeTokens;
    
    //! Is spike recording enabled for this population?
    bool m_SpikeRecordingEnabled;

    //! Is spike event recording enabled?
    bool m_SpikeEventRecordingEnabled;
};
}   // namespace GeNN
