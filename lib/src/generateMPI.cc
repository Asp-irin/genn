/*--------------------------------------------------------------------------
  Author: Mengchi Zhang
  
  Institute: Center for Computational Neuroscience and Robotics
  University of Sussex
  Falmer, Brighton BN1 9QJ, UK 
  
  email to:  zhan2308@purdue.edu
  
  initial version: 2017-07-19
  
  --------------------------------------------------------------------------*/

//-----------------------------------------------------------------------
/*!  \file generateMPI.cc

  \brief Contains functions to generate code for running the
  simulation with MPI. Part of the code generation section.
*/
//--------------------------------------------------------------------------

#include "generateMPI.h"

// Standard C++ includes
#include <fstream>

// GeNN includes
#include "codeStream.h"
#include "modelSpec.h"

//--------------------------------------------------------------------------
// Anonymous namespace
//--------------------------------------------------------------------------
namespace
{
void genHeader(const NNmodel &model,    //!< Model description
               const string &path)      //!< Path for code generationn
{
    //=======================
    // generate mpi.h
    //=======================

    // this file contains helpful macros and is separated out so that it can also be used by other code that is compiled separately
    string name= model.getGeneratedCodePath(path, "mpi.h");
    ofstream fs;
    fs.open(name.c_str());

    // Attach this to a code stream
    CodeStream os(fs);

    writeHeader(os);
    os << std::endl;

    // write doxygen comment
    os << "//-------------------------------------------------------------------------" << std::endl;
    os << "/*! \\file infraMPI.h" << std::endl << std::endl;
    os << "\\brief File generated from GeNN for the model " << model.getName() << " containing MPI function definition." << std::endl;
    os << "*/" << std::endl;
    os << "//-------------------------------------------------------------------------" << std::endl << std::endl;

    os << "#ifndef INFRAMPI_H" << std::endl;
    os << "#define INFRAMPI_H" << std::endl;
    os << std::endl;

#ifdef MPI_ENABLE
    os << "#include <mpi.h>" << std::endl;
#endif

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// copying things to remote" << std::endl;
    os << std::endl;
    for(const auto &n : model.getLocalNeuronGroups()) {
        os << "void push" << n.first << "SpikesToRemote(int remote);" << std::endl;
    }
    os << std::endl;

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// copying things from remote" << std::endl;
    os << std::endl;
    for(const auto &n : model.getRemoteNeuronGroups()) {
        os << "void pull" << n.first << "SpikesFromRemote(int remote, int tag);" << std::endl;
    }
    os << std::endl;

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// global copying spikes to remote" << std::endl;
    os << std::endl;
    os << "void copySpikesToRemote(int remote, int tag);" << std::endl;
    os << std::endl;

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// global copying spikes from remote" << std::endl;
    os << std::endl;
    os << "void copySpikesFromRemote(int remote, int tag);" << std::endl;
    os << std::endl;

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// global spikes communication" << std::endl;
    os << std::endl;
    os << "void communicateSpikes();" << std::endl;
    os << std::endl;

    os << "#endif" << std::endl;
    fs.close();
}

void genCode(const NNmodel &model,  //!< Model description
             const string &path,    //!< Path for code generationn
             int localHostID)       //!< ID of local host
{
    //=======================
    // generate mpi.cc
    //=======================
    string name= model.getGeneratedCodePath(path, "mpi.cc");
    ofstream fs;
    fs.open(name.c_str());

    // Attach this to a code stream
    CodeStream os(fs);

    writeHeader(os);
    os << std::endl;

    // write doxygen comment
    os << "//-------------------------------------------------------------------------" << std::endl;
    os << "/*! \\file inftraMPI.cc" << std::endl << std::endl;
    os << "\\brief File generated from GeNN for the model " << model.getName() << " containing MPI infrastructure code." << std::endl;
    os << "*/" << std::endl;
    os << "//-------------------------------------------------------------------------" << std::endl;
    os << std::endl;

#ifdef MPI_ENABLE
    os << "#include <mpi.h>" << std::endl;
#endif
    os << std::endl;

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// copying spikes to remote" << std::endl << std::endl;

    for(const auto &n : model.getLocalNeuronGroups()) {
        // neuron spike variables
        os << "void push" << n.first << "SpikesToRemote(int remote, int tag)" << std::endl;
        os << CodeStream::OB(1050);
        os << "MPI_Request req;" << std::endl;

        const size_t glbSpkCntSize = n.second.isTrueSpikeRequired() ? n.second.getNumDelaySlots() : 1;
        os << "MPI_Isend(glbSpkCnt" << n.first;
        os << ", "<< glbSpkCntSize;
        os << ", MPI_UNSIGNED";
        os << ", remote, tag, MPI_COMM_WORLD, &req);" << std::endl;

        const size_t glbSpkSize = n.second.isTrueSpikeRequired() ? n.second.getNumNeurons() * n.second.getNumDelaySlots() : n.second.getNumNeurons();
        os << "MPI_Isend(glbSpk" << n.first;
        os << ", "<< glbSpkSize;
        os << ", MPI_UNSIGNED";
        os << ", remote, tag, MPI_COMM_WORLD, &req);" << std::endl;

        os << CodeStream::CB(1050);
        os << std::endl;
    }

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// copying spikes from remote" << std::endl << std::endl;

    for(const auto &n : model.getRemoteNeuronGroups()) {
        // neuron spike variables
        os << "void pull" << n.first << "SpikesFromRemote(int remote, int tag)" << std::endl;
        os << CodeStream::OB(1051);

        const size_t glbSpkCntSize = n.second.isTrueSpikeRequired() ? n.second.getNumDelaySlots() : 1;
        os << "MPI_Recv(glbSpkCnt" << n.first;
        os << ", "<< glbSpkCntSize;
        os << ", MPI_UNSIGNED";
        os << ", remote, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);" << std::endl;

        const size_t glbSpkSize = n.second.isTrueSpikeRequired() ? n.second.getNumNeurons() * n.second.getNumDelaySlots() : n.second.getNumNeurons();
        os << "MPI_Recv(glbSpk" << n.first;
        os << ", "<< glbSpkSize;
        os << ", MPI_UNSIGNED";
        os << ", remote, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);" << std::endl;

        os << CodeStream::CB(1051);
        os << std::endl;
    }
    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// global copying spikes to remote" << std::endl << std::endl;

    os << "void copySpikesToRemote(int remote, int tag)" << std::endl;
    os << CodeStream::OB(1052);
    for(const auto &n : model.getLocalNeuronGroups()) {
        os << "push" << n.first << "SpikesToRemote(remote, tag);" << std::endl;
    }
    os << CodeStream::CB(1052);
    os << std::endl;

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// global copying spikes from remote" << std::endl << std::endl;

    os << "void copySpikesFromRemote(int remote, int tag)" << std::endl;
    os << CodeStream::OB(1053) << std::endl;

    for(const auto &n : model.getRemoteNeuronGroups()) {
      os << "pull" << n.first << "SpikesFromRemote(remote, tag);" << std::endl;
    }
    os << CodeStream::CB(1053);
    os << std::endl;

    os << "// ------------------------------------------------------------------------" << std::endl;
    os << "// communication function to sync spikes" << std::endl << std::endl;

    os << "void communicateSpikes()" << std::endl;
    os << CodeStream::OB(1054) << std::endl;

    os << "int localID;" << std::endl;
    os << "MPI_Comm_rank(MPI_COMM_WORLD, &localID);" << std::endl;

    for(const auto &n : model.getLocalNeuronGroups()) {
        os << "// Neuron group '" << n.first << "' - outgoing connections" << std::endl;
        for(auto *s : n.second.getOutSyn()) {
            // If the TARGET neuron group is not running on this machine
            const int trgClusterHostID = s->getTrgNeuronGroup()->getClusterHostID();
            if (trgClusterHostID != localHostID) {
                os << "// send to synapse" << s->getName()<< std::endl;
                os << "copySpikesToRemote(" << trgClusterHostID << ", " << (hashString(n.first) & 0x7FFFFFFF) << ");" << std::endl;
            }
        }
    }
    for(const auto &n : model.getLocalNeuronGroups()) {
        os << "// Neuron group '" << n.first << "' - incoming connections" << std::endl;
        for(auto *s : n.second.getInSyn()) {
            // If the SOURCE neuron group is not running on this machine
            const int srcClusterHostID = s->getSrcNeuronGroup()->getClusterHostID();
            if (srcClusterHostID != localHostID) {
                os << "// receive from synapse" << s->getName() << " " << s->getSrcNeuronGroup()->getName() << std::endl;
                os << "copySpikesFromRemote(" << srcClusterHostID << ", " << (hashString(s->getSrcNeuronGroup()->getName()) & 0x7FFFFFFF) << ");" << std::endl;
            }
        }
    }
    os << CodeStream::CB(1054);
    os << std::endl;

    fs.close();
}
}   // Anonymous namespace

//--------------------------------------------------------------------------
/*!
  \brief A function that generates predominantly MPI infrastructure code.

  In this function MPI infrastructure code are generated,
  including: MPI send and receive functions.
*/
//--------------------------------------------------------------------------
void genMPI(const NNmodel &model,   //!< Model description
            const string &path,     //!< Path for code generation
            int localHostID)        //!< ID of local host
{
    genHeader(model, path);
    genCode(model, path, localHostID);
}
