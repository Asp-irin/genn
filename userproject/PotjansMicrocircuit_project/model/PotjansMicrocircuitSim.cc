// Standard C++ includes
#include <memory>
#include <random>
#include <vector>

// GeNN user project includes
#include "../include/fixedNumberTotalPreCalc.h"
#include "../include/sharedLibraryModel.h"
#include "../include/spikeRecorder.h"
#include "../include/timer.h"

// Model parameters
#include "PotjansMicrocircuitParams.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "usage: PotjansMicrocircuit <basename>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string outLabel = argv[1];
    const std::string outDir = "../" + outLabel + "_output";

    SharedLibraryModel<float> model("./", "PotjansMicrocircuit");

    model.allocateMem();

    {
        Timer timer("Building row lengths:");

        std::mt19937 rng;
        for(unsigned int trgLayer = 0; trgLayer < Parameters::LayerMax; trgLayer++) {
            for(unsigned int trgPop = 0; trgPop < Parameters::PopulationMax; trgPop++) {
                const std::string trgName = Parameters::getPopulationName(trgLayer, trgPop);
                const unsigned int numTrg = Parameters::getScaledNumNeurons(trgLayer, trgPop);

                // Loop through source populations and layers
                for(unsigned int srcLayer = 0; srcLayer < Parameters::LayerMax; srcLayer++) {
                    for(unsigned int srcPop = 0; srcPop < Parameters::PopulationMax; srcPop++) {
                        const std::string srcName = Parameters::getPopulationName(srcLayer, srcPop);
                        const unsigned int numSrc = Parameters::getScaledNumNeurons(srcLayer, srcPop);

                        // If synapse group exists
                        const std::string synapsePopName = srcName + "_" + trgName;
                        void *rowLengths = model.getSymbol("preCalcRowLength" + synapsePopName, true);
                        if(rowLengths) {
                            // Allocate row lengths
                            model.allocateExtraGlobalParam(synapsePopName, "preCalcRowLength", numSrc);

                            // Build row lengths on host
                            preCalcRowLengths(numSrc, numTrg, Parameters::getScaledNumConnections(srcLayer, srcPop, trgLayer, trgPop),
                                             *(static_cast<unsigned int**>(rowLengths)), rng);

                            // Push to device
                            model.pushExtraGlobalParam(synapsePopName, "preCalcRowLength", numSrc);
                        }
                    }
                }
            }
        }
    }

    model.initialize();
    model.initializeSparse();

    double recordS = 0.0;
    {
        // Create spike recorders
        std::vector<SpikeRecorder<SpikeWriterTextCached>> spikeRecorders;
        spikeRecorders.reserve(Parameters::LayerMax * Parameters::PopulationMax);
        for(unsigned int layer = 0; layer < Parameters::LayerMax; layer++) {
            for(unsigned int pop = 0; pop < Parameters::PopulationMax; pop++) {
                const std::string name = Parameters::getPopulationName(layer, pop);
                spikeRecorders.push_back(model.getSpikeRecorder<SpikeWriterTextCached>(name,  outDir + "/" + outLabel + "." + name + ".st"));
            }
        }

        Timer timer("Simulation:");
        // Loop through timesteps
        const unsigned int timesteps = round(Parameters::durationMs / Parameters::dtMs);
        const unsigned int tenPercentTimestep = timesteps / 10;
        for(unsigned int i = 0; i < timesteps; i++) {
            // Indicate every 10%
            if((i % tenPercentTimestep) == 0) {
                std::cout << i / 100 << "%" << std::endl;
            }

            // Simulate
            model.stepTime();

            // Pull spikes from each population from device
            for(unsigned int layer = 0; layer < Parameters::LayerMax; layer++) {
                for(unsigned int pop = 0; pop < Parameters::PopulationMax; pop++) {
                    model.pullCurrentSpikesFromDevice(Parameters::getPopulationName(layer, pop));
                }
            }

            {
                TimerAccumulate timer(recordS);

                // Record spikes
                for(auto &s : spikeRecorders) {
                    s.record(model.getTime());
                }
            }
        }
    }

    if(Parameters::measureTiming) {
        std::cout << "Timing:" << std::endl;
        std::cout << "\tInit:" << *model.getScalar<double>("initTime") * 1000.0 << std::endl;
        std::cout << "\tSparse init:" << *model.getScalar<double>("initSparseTime") * 1000.0 << std::endl;
        std::cout << "\tNeuron simulation:" << *model.getScalar<double>("neuronUpdateTime") * 1000.0 << std::endl;
        std::cout << "\tSynapse simulation:" << *model.getScalar<double>("presynapticUpdateTime") * 1000.0 << std::endl;
    }
    std::cout << "Record:" << recordS << "s" << std::endl;

    return EXIT_SUCCESS;
}
