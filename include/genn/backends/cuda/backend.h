#pragma once

// Standard C++ includes
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <numeric>
#include <string>

// Standard C includes
#include <cassert>

// CUDA includes
#include <cuda.h>
#include <cuda_runtime.h>

#if __has_include(<nccl.h>)
    #include <nccl.h>

    #define NCCL_AVAILABLE
#endif

// GeNN includes
#include "backendExport.h"

// GeNN code generator includes
#include "code_generator/backendCUDAHIP.h"
#include "code_generator/codeStream.h"

// Forward declarations
namespace filesystem
{
    class path;
}

//--------------------------------------------------------------------------
// GeNN::CodeGenerator::CUDA::DeviceSelectMethod
//--------------------------------------------------------------------------
namespace GeNN::CodeGenerator::CUDA
{
//! Methods for selecting CUDA device
enum class DeviceSelect
{
    OPTIMAL,        //!< Pick optimal device based on how well kernels can be simultaneously simulated and occupancy
    MOST_MEMORY,    //!< Pick device with most global memory
    MANUAL,         //!< Use device specified by user
};

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::BlockSizeSelect
//--------------------------------------------------------------------------
//! Methods for selecting CUDA kernel block size
enum class BlockSizeSelect
{
    OCCUPANCY,  //!< Pick optimal blocksize for each kernel based on occupancy
    MANUAL,     //!< Use block sizes specified by user
};

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::Preferences
//--------------------------------------------------------------------------
//! Preferences for CUDA backend
struct Preferences : public PreferencesCUDAHIP
{
    Preferences()
    {
        std::fill(manualBlockSizes.begin(), manualBlockSizes.end(), 32);
    }

    //! Should PTX assembler information be displayed for each CUDA kernel during compilation?
    bool showPtxInfo = false;

    //! Should line info be included in resultant executable for debugging/profiling purposes?
    bool generateLineInfo = false;

    //! How to select GPU device
    DeviceSelect deviceSelectMethod = DeviceSelect::MANUAL;

    //! If device select method is set to DeviceSelect::MANUAL, id of device to use
    unsigned int manualDeviceID = 0;

    //! How to select CUDA blocksize
    BlockSizeSelect blockSizeSelectMethod = BlockSizeSelect::OCCUPANCY;

    //! If block size select method is set to BlockSizeSelect::MANUAL, block size to use for each kernel
    KernelBlockSize manualBlockSizes;

    //! How much constant cache is already used and therefore can't be used by GeNN?
    /*! Each of the four modules which includes CUDA headers(neuronUpdate, synapseUpdate, custom update, init and runner)
        Takes 72 bytes of constant memory for a lookup table used by cuRAND. If your application requires
        additional constant cache, increase this */
    size_t constantCacheOverhead = 72 * 5;

    //! NVCC compiler options for all GPU code
    std::string userNvccFlags = "";

    void updateHash(boost::uuids::detail::sha1 &hash) const
    {
        // Superclass 
        PreferencesCUDAHIP::updateHash(hash);

        // **NOTE** showPtxInfo, generateLineInfo and userNvccFlags only affect makefiles/msbuild 
        // **NOTE** block size optimization is also not relevant, the chosen block size is hashed in the backend
        // **NOTE** while device selection is also not relevant as the chosen device is hashed in the backend, DeviceSelect::MANUAL_OVERRIDE is used in the backend

        //! Update hash with preferences
        Utils::updateHash(deviceSelectMethod, hash);
        Utils::updateHash(constantCacheOverhead, hash);
    }
};

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::Backend
//--------------------------------------------------------------------------
class BACKEND_EXPORT State : public Runtime::StateBase
{
public:
    State(const Runtime::Runtime &base);

    //------------------------------------------------------------------------
    // Public API
    //------------------------------------------------------------------------
    //! To be called on one rank to generate ID before creating communicator
    void ncclGenerateUniqueID();
    
    //! Get pointer to unique ID
    unsigned char *ncclGetUniqueID();
    
    //! Get size of unique ID in bytes
    size_t ncclGetUniqueIDSize() const;

    //! Initialise communicator
    void ncclInitCommunicator(int rank, int numRanks);

private:
    //----------------------------------------------------------------------------
    // Type defines
    //----------------------------------------------------------------------------
    typedef void (*VoidFunction)(void);
    typedef unsigned char* (*BytePtrFunction)(void);
    typedef void (*NCCLInitCommunicatorFunction)(int, int);

    //------------------------------------------------------------------------
    // Members
    //------------------------------------------------------------------------
    VoidFunction m_NCCLGenerateUniqueID;
    BytePtrFunction m_NCCLGetUniqueID;
    NCCLInitCommunicatorFunction m_NCCLInitCommunicator;
    const size_t *m_NCCLUniqueIDSize;
};

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::Backend
//--------------------------------------------------------------------------
class BACKEND_EXPORT Backend : public BackendCUDAHIP
{
public:
    Backend(const KernelBlockSize &kernelBlockSizes, const Preferences &preferences, 
            int device, bool zeroCopy);

    //--------------------------------------------------------------------------
    // CodeGenerator::BackendSIMT virtuals
    //--------------------------------------------------------------------------
    //! How many 'lanes' does underlying hardware have?
    /*! This is typically used for warp-shuffle algorithms */
    virtual unsigned int getNumLanes() const final;

    //! Create backend-specific runtime state object
    /*! \param runtime  runtime object */
    virtual std::unique_ptr<GeNN::Runtime::StateBase> createState(const Runtime::Runtime &runtime) const final;

    //! Create backend-specific array object
    /*! \param type         data type of array
        \param count        number of elements in array, if non-zero will allocate
        \param location     location of array e.g. device-only*/
    virtual std::unique_ptr<Runtime::ArrayBase> createArray(const Type::ResolvedType &type, size_t count, 
                                                            VarLocation location, bool uninitialized) const final;

    //! Create array of backend-specific population RNGs (if they are initialised on host this will occur here)
    /*! \param count        number of RNGs required*/
    virtual std::unique_ptr<Runtime::ArrayBase> createPopulationRNG(size_t count) const final;

    virtual void genMakefilePreamble(std::ostream &os) const final;
    virtual void genMakefileLinkRule(std::ostream &os) const final;
    virtual void genMakefileCompileRule(std::ostream &os) const final;

    virtual void genMSBuildConfigProperties(std::ostream &os) const final;
    virtual void genMSBuildImportProps(std::ostream &os) const final;
    virtual void genMSBuildItemDefinitions(std::ostream &os) const final;
    virtual void genMSBuildCompileModule(const std::string &moduleName, std::ostream &os) const final;
    virtual void genMSBuildImportTarget(std::ostream &os) const final;

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------
    const cudaDeviceProp &getChosenCUDADevice() const{ return m_ChosenDevice; }
    int getChosenDeviceID() const{ return m_ChosenDeviceID; }
    int getRuntimeVersion() const{ return m_RuntimeVersion; }
    std::string getNVCCFlags() const;

private:
    //--------------------------------------------------------------------------
    // Members
    //--------------------------------------------------------------------------
    const int m_ChosenDeviceID;
    cudaDeviceProp m_ChosenDevice;
    int m_RuntimeVersion;
};
}   // GeNN::CUDA::CodeGenerator
