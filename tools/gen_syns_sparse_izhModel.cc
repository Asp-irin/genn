//--------------------------------------------------------------------------
/*! \file gen_syns_sparse_izhModel.cc

\brief This file is part of a tool chain for running the Izhikevich network model.

*/ 
//--------------------------------------------------------------------------
//gdb -tui --args ./gen_syns_sparse_izhModel 1000 1000 0.5 -1 izh
//g++ -Wall -Winline -g -I../lib/include/numlib -o gen_syns_sparse_izhModel gen_syns_sparse_izhModel.cc


using namespace std;

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "randomGen.h"
#include "randomGen.cc"

int printVector(vector<unsigned int>&);
int printVector(vector<float>&);

randomGen R;
randomGen Rind;

float gsyn;
//  float *gAlltoAll;
float *garray; 
//  float *g; 
unsigned int *postInd; 
  
  //exc-exc
  //float *gAlltoAll_ee;
  float *garray_ee; 
  //float *g_ee = new float[nConn*nExc]; //same here for writing to file
  std::vector<float> g_ee;
  std::vector<unsigned int> postIndInG_ee;
  std::vector<unsigned int> postInd_ee;
  //int maxInColI_ee;
  
  //exc-inh
 // float *gAlltoAll_ei;
  float *garray_ei;
  std::vector<float> g_ei;
  std::vector<unsigned int> postIndInG_ei;
  std::vector<unsigned int> postInd_ei;
  //int maxInColI_ei;
 
  //inh-exc
  //float *gAlltoAll_ie;
  float *garray_ie;
  std::vector<float> g_ie;
  std::vector<unsigned int> postIndInG_ie;
  std::vector<unsigned int> postInd_ie;
  //int maxInColI_ie;
 
  //inh-inh
  //float *gAlltoAll_ii;
  float *garray_ii;
  std::vector<float> g_ii;
  std::vector<unsigned int> postIndInG_ii;
  std::vector<unsigned int> postInd_ii;
  //int maxInColI_ii;

int main(int argc, char *argv[])
{
  if (argc != 6)
  {
    cerr << "usage: gen_syns_sparse_izhModel <nNeurons> <nConnPerNeuron> <meanSExc> <meanSInh>";
    cerr << " <outfile>" << endl;
    exit(1);
  }
  
  unsigned int nN= atoi(argv[1]);
  unsigned int nExc= (int)(4*nN/5);
  //unsigned int nInh= nN-nExc;
  unsigned int nConn= atoi(argv[2]);
  float meangsynExc= atof(argv[3]);
  float meangsynInh= atof(argv[4]);


  //alltogether
  char filename[100];

  strcpy(filename,argv[5]);

  //ee
    
  char filename_ee[100];
  char filename_index_ee[100];
  char filename_postindex_ee[100]; 
  //char filename_nonopt_ee[100];
  char filename_info_ee[100];  
  
  strcpy(filename_ee,filename);
  strcat(filename_ee,"_ee");
    
  strcpy(filename_index_ee,filename);
  strcat(filename_index_ee,"_postind_ee");

  strcpy(filename_postindex_ee,filename);
  strcat(filename_postindex_ee,"_postIndInG_ee");

  /*strcpy(filename_nonopt_ee,filename);
  strcat(filename_nonopt_ee,"_nonopt_ee");
*/
  strcpy(filename_info_ee,filename);
  strcat(filename_info_ee,"_info_ee");
  
  ofstream os_ee(filename_ee, ios::binary);
  ofstream os_index_ee(filename_index_ee, ios::binary);
  ofstream os_postindex_ee(filename_postindex_ee, ios::binary);
  //ofstream os_nonopt_ee(filename_nonopt_ee, ios::binary);
  ofstream os_info_ee(filename_info_ee, ios::binary);
  
  //ei
  char filename_ei[100];
  char filename_index_ei[100];
  char filename_postindex_ei[100]; 
  //char filename_nonopt_ei[100];
  char filename_info_ei[100];  
  
  strcpy(filename_ei,filename);
  strcat(filename_ei,"_ei");
  
  strcpy(filename_index_ei,filename);
  strcat(filename_index_ei,"_postind_ei");

  strcpy(filename_postindex_ei,filename);
  strcat(filename_postindex_ei,"_postIndInG_ei");

  /*strcpy(filename_nonopt_ei,filename);
  strcat(filename_nonopt_ei,"_nonopt_ei");*/

  strcpy(filename_info_ei,filename);
  strcat(filename_info_ei,"_info_ei");
  
  ofstream os_ei(filename_ei, ios::binary);
  ofstream os_index_ei(filename_index_ei, ios::binary);
  ofstream os_postindex_ei(filename_postindex_ei, ios::binary);
  //ofstream os_nonopt_ei(filename_nonopt_ei, ios::binary);
  ofstream os_info_ei(filename_info_ei, ios::binary);
  
  //ie
  char filename_ie[100];
  char filename_index_ie[100];
  char filename_postindex_ie[100]; 
  //char filename_nonopt_ie[100];
  char filename_info_ie[100];  
  
  strcpy(filename_ie,filename);
  strcat(filename_ie,"_ie");
  
  strcpy(filename_index_ie,filename);
  strcat(filename_index_ie,"_postind_ie");

  strcpy(filename_postindex_ie,filename);
  strcat(filename_postindex_ie,"_postIndInG_ie");

  /*strcpy(filename_nonopt_ie,filename);
  strcat(filename_nonopt_ie,"_nonopt_ie");*/

  strcpy(filename_info_ie,filename);
  strcat(filename_info_ie,"_info_ie");
  
  ofstream os_ie(filename_ie, ios::binary);
  ofstream os_index_ie(filename_index_ie, ios::binary);
  ofstream os_postindex_ie(filename_postindex_ie, ios::binary);
  //ofstream os_nonopt_ie(filename_nonopt_ie, ios::binary);
  ofstream os_info_ie(filename_info_ie, ios::binary);
    
  //ii
  char filename_ii[100];
  char filename_index_ii[100];
  char filename_postindex_ii[100]; 
  //char filename_nonopt_ii[100];
  char filename_info_ii[100];  
  
      
  strcpy(filename_ii,filename);
  strcat(filename_ii,"_ii");
  
  strcpy(filename_index_ii,filename);
  strcat(filename_index_ii,"_postind_ii");

  strcpy(filename_postindex_ii,filename);
  strcat(filename_postindex_ii,"_postIndInG_ii");

  /*strcpy(filename_nonopt_ii,filename);
  strcat(filename_nonopt_ii,"_nonopt_ii");*/

  strcpy(filename_info_ii,filename);
  strcat(filename_info_ii,"_info_ii");
  
  ofstream os_ii(filename_ii, ios::binary);
  ofstream os_index_ii(filename_index_ii, ios::binary);
  ofstream os_postindex_ii(filename_postindex_ii, ios::binary);
  //ofstream os_nonopt_ii(filename_nonopt_ii, ios::binary);
  ofstream os_info_ii(filename_info_ii, ios::binary);  
  
  //gAlltoAll = new float[nN*nN];
  garray = new float[nConn]; 
  //g = new float[nConn*nN]; 
  postInd = new unsigned int[nConn*nN]; 
  
  //exc-exc
  //gAlltoAll_ee = new float[nExc*nExc];
  garray_ee = new float[nConn]; 
  //float *g_ee = new float[nConn*nExc]; //same here for writing to file
  std::vector<float> g_ee;
  //unsigned int *postIndInG_ee = new unsigned int[nExc+1];
  std::vector<unsigned int> postIndInG_ee;
  //unsigned int *postInd_ee = new unsigned int[nConn*nExc]; //same here for writing to file
  std::vector<unsigned int> postInd_ee;
  //int maxInColI_ee;
  
  //exc-inh
  //gAlltoAll_ei = new float[nExc*nInh];
  garray_ei = new float[nConn];
  //float *g_ei = new float[nConn*nExc];
  std::vector<float> g_ei;
  //unsigned int *postIndInG_ei = new unsigned int[nExc+1];
  std::vector<unsigned int> postIndInG_ei;
  //unsigned int *postInd_ei = new unsigned int[nConn*nExc];
  std::vector<unsigned int> postInd_ei;
  //int maxInColI_ei;
 
  //inh-exc
 // gAlltoAll_ie = new float[nInh*nExc];
  garray_ie = new float[nConn];
  //float *g_ie = new float[nConn*nInh];
  std::vector<float> g_ie;
  //unsigned int *postIndInG_ie = new unsigned int[nInh+1];
  std::vector<unsigned int> postIndInG_ie;
  //unsigned int *postInd_ie = new unsigned int[nConn*nInh]; 
  std::vector<unsigned int> postInd_ie;
  //int maxInColI_ie;
 
  //inh-inh
 // gAlltoAll_ii = new float[nInh*nInh];
  garray_ii = new float[nConn];
  //float *g_ii = new float[nConn*nInh];
  std::vector<float> g_ii;
  //unsigned int *postIndInG_ii = new unsigned int[nInh+1];
  std::vector<unsigned int> postIndInG_ii;
  //unsigned int *postInd_ii = new unsigned int[nConn*nInh];
  std::vector<unsigned int> postInd_ii;
  //int maxInColI_ii;
  
  cerr << "# call was: ";
  for (int i = 0; i < argc; i++) cerr << argv[i] << " ";
  cerr << endl;

  postIndInG_ee.push_back(0);
  postIndInG_ei.push_back(0); 
  postIndInG_ie.push_back(0); 
  postIndInG_ii.push_back(0);  
  
  /*int size_ee = 0; //counter for the size of the exc-exc synapse
  int size_ei = 0; //counter for the size of the exc-inh synapse
  int size_ie = 0; //counter for the size of the inh-exc synapse
  int size_ii = 0; //counter for the size of the inh-inh synapse*/
  
  unsigned int sum_ee=0;
  unsigned int sum_ei=0;
  unsigned int sum_ie=0;
  unsigned int sum_ii=0;

  //number of pre-to-post is controlled but post-to-pre should be controlled by counting the number of connections for each postsynaptic neuron
  unsigned int *precount = new unsigned int[nN]; 

  for (unsigned int i= 0; i < nN; i++) {
  	precount[i]=0;
  }


  for (unsigned int i= 0; i < nN; i++) {
 		//reservoir sampling to choose nConn random connections for each neuron
 		for (unsigned int j=0 ; j< nConn; j++){
    	//postIndex[j]=j;
    	gsyn=R.n();
    	
    	if (i<nExc) {
        	gsyn*=meangsynExc;
        }
        else{
        	gsyn*=meangsynInh;
        }
    	garray[j]=gsyn;
   	postInd[i*nConn+j] = j;
	   // gAlltoAll[i*nN+j] = gsyn;*/
	    precount[j]++;
    }
    for (unsigned int j=nConn ; j< nN; j++){
    	postInd[j]=j;
    }
    for (unsigned int j=nConn; j< nN; j++){
			unsigned int rn = (unsigned int)(R.n()*(j+1));
      if (rn<nConn){
        /*if (precount[j]>nConn){
          cout << "postsynaptic neuron " << j << " has more than " << nConn << " connections..." << endl;
        }*/
        gsyn=R.n();
        if (i<nExc) {
        	gsyn*=meangsynExc;
        }
        else{
        	gsyn*=meangsynInh;
        }
        //cerr << i << ": create a connection for " << j << " to replace " << rn << endl; 
        //if (gAlltoAll[i*nN+postInd[i*nConn+rn]]==0) 
        precount[postInd[i*nConn+rn]]--;
     //   gAlltoAll[i*nN+postInd[i*nConn+rn]]=0;
        precount[j]++;
        postInd[i*nConn+rn]=j;
	      garray[rn]=gsyn;
	   //   gAlltoAll[i*nN+j]=gsyn;
	   //   gAlltoAll[i*nN+rn]=0;
      }
     /* else{
      	gAlltoAll[i*nN+rn]=gsyn;
      }*/
    }   
    
    //connectivity is set for the presynaptic neuron. Now push it to subgroups of populations

   	//cout << "nexc: " << nExc<< ", nInh: " << nInh << endl;
 /*   for (int p=0;p<nConn*nN;p++){
    if (p%nConn==0) cout << " for line "<< p <<"\n";
  	cout << postInd[p] << " ";
  }*/
    for (unsigned int j=0 ; j< nConn; j++){
      if ((i<nExc)&&(postInd[i*nConn+j]<nExc)){ //exc-exc
        g_ee.push_back(garray[j]);
        //gAlltoAll_ee[i*nExc+postInd[i*nConn+j]]=garray[j];
        postInd_ee.push_back(postInd[i*nConn+j]);
        sum_ee++;
      }
      
      if ((i<nExc)&&(postInd[i*nConn+j]>=nExc)){ //exc-inh
        g_ei.push_back(garray[j]);
        //gAlltoAll_ei[i*nInh+(postInd[i*nConn+j]-nExc)]=garray[j];
        postInd_ei.push_back(postInd[i*nConn+j]-nExc);
        sum_ei++;
      }
      
      if ((i>=nExc)&&(postInd[i*nConn+j]<nExc)){ //inh-exc
        g_ie.push_back(garray[j]);
        //gAlltoAll_ie[(i-nExc)*nExc+(postInd[i*nConn+j])]=garray[j];
        postInd_ie.push_back(postInd[i*nConn+j]);
        sum_ie++;
      }
      
      if ((i>=nExc)&&(postInd[i*nConn+j]>=nExc)){ //inh-inh
        g_ii.push_back(garray[j]);
        //gAlltoAll_ii[(i-nExc)*nInh+(postInd[i*nConn+j]-nExc)]=garray[j];
        postInd_ii.push_back(postInd[i*nConn+j]-nExc);
        sum_ii++;
      }
    }
    
    if (i<nExc){
    	postIndInG_ee.push_back(sum_ee);
    	postIndInG_ei.push_back(sum_ei);
    }
    else{ 
   		postIndInG_ie.push_back(sum_ie); 
			postIndInG_ii.push_back(sum_ii);
		}
		
//    memcpy(g+i*nConn,garray,nConn*sizeof(float));   
	     //gOld[i*nMB+j]= 0.0f;
  }
 

  //os.write((char *)g, nN*nConn*sizeof(float));
  //os_nonopt.write((char *)gOld, nN*nN*sizeof(float));
  //os.close();
  
  //os_index.write((char *)postInd, nN*nConn*sizeof(unsigned int));
  //os_postindex.write((char *)postIndInG, nN*sizeof(unsigned int));
  //os_nonopt.write((char *)gAlltoAll, nN*nN*sizeof(float));
  
 //os_index.close();
  //os_postindex.close();
  //os_nonopt.close();
 
  /*cout << "\nprinting g:\n" << endl; 
  for (unsigned int j=0;j<nConn*nN;j++){
    if (j%nConn==0) cout << "\n";
  	cout << g[j] << " ";
  }*/
  
  /*float *gAlltoAll_ee = new float[nExc*nExc];
  float *garray_ee = new float[nConn]; 
  //float *g_ee = new float[nConn*nExc]; //same here for writing to file
  std::vector<float> g_ee;
  //unsigned int *postIndInG_ee = new unsigned int[nExc+1];
  std::vector<unsigned int> postIndInG_ee;
  //unsigned int *postInd_ee = new unsigned int[nConn*nExc]; //same here for writing to file
  std::vector<unsigned int> postInd_ee;*/
 
 
  //ee
  size_t sz = g_ee.size();
  cout << "ee vect.size: " << sz << endl;
  os_info_ee.write((char *)&sz,sizeof(size_t));
  os_ee.write(reinterpret_cast<const char*>(&g_ee[0]), sz * sizeof(g_ee[0]));
  sz = postInd_ee.size();
  cout << "ee ind size: " << sz << endl;
  os_index_ee.write(reinterpret_cast<const char*>(&postInd_ee[0]), sz * sizeof(postInd_ee[0]));
  sz = postIndInG_ee.size();
  cout << "ee count size: " << sz << endl;
  os_postindex_ee.write(reinterpret_cast<const char*>(&postIndInG_ee[0]), sz * sizeof(postIndInG_ee[0]));
  //os_nonopt_ee.write((char *)gAlltoAll_ee, nExc*nExc*sizeof(float));
  
  os_ee.close();
  os_index_ee.close();
  os_postindex_ee.close();
  //os_nonopt_ee.close();
  os_info_ee.close();
  
  //ei
  sz = g_ei.size();
  cout << "ei vect.size: " << sz << endl;
  os_info_ei.write((char *)&sz,sizeof(size_t));
  os_ei.write(reinterpret_cast<const char*>(&g_ei[0]), sz * sizeof(g_ei[0]));
  sz = postInd_ei.size();
  cout << "ei ind size: " << sz << endl;
  os_index_ei.write(reinterpret_cast<const char*>(&postInd_ei[0]), sz * sizeof(postInd_ei[0]));
  sz = postIndInG_ei.size();
  cout << "ei count size: " << sz << endl;
  os_postindex_ei.write(reinterpret_cast<const char*>(&postIndInG_ei[0]), sz * sizeof(postIndInG_ei[0]));
  //os_nonopt_ei.write((char *)gAlltoAll_ei, nExc*nInh*sizeof(float));
  
  os_ei.close();
  os_index_ei.close();
  os_postindex_ei.close();
  //os_nonopt_ei.close();
  os_info_ei.close();
  
  //ie
  sz = g_ie.size();
  cout << "ie vect.size: " << sz << endl;
  os_info_ie.write((char *)&sz,sizeof(size_t));
  os_ie.write(reinterpret_cast<const char*>(&g_ie[0]), sz * sizeof(g_ie[0]));
  sz = postInd_ie.size();
  cout << "ie ind size: " << sz << endl;
  os_index_ie.write(reinterpret_cast<const char*>(&postInd_ie[0]), sz * sizeof(postInd_ie[0]));
  sz = postIndInG_ie.size();
  cout << "ie count size: " << sz << endl;
  os_postindex_ie.write(reinterpret_cast<const char*>(&postIndInG_ie[0]), sz * sizeof(postIndInG_ie[0]));
  //os_nonopt_ie.write((char *)gAlltoAll_ie, nInh*nExc*sizeof(float));
  
  os_ie.close();
  os_index_ie.close();
  os_postindex_ie.close();
  //os_nonopt_ie.close();
  os_info_ie.close();
  
  //ii
  sz = g_ii.size();
  cout << "ii vect.size: " << sz << endl;
  os_info_ii.write((char *)&sz,sizeof(size_t));
  os_ii.write(reinterpret_cast<const char*>(&g_ii[0]), sz * sizeof(g_ii[0]));
  sz = postInd_ii.size();
  cout << "ii ind size: " << sz << endl;
  os_index_ii.write(reinterpret_cast<const char*>(&postInd_ii[0]), sz * sizeof(postInd_ii[0]));
  sz = postIndInG_ii.size();
  cout << "ii count size: " << sz << endl;
  os_postindex_ii.write(reinterpret_cast<const char*>(&postIndInG_ii[0]), sz * sizeof(postIndInG_ii[0]));
  //os_nonopt_ii.write((char *)gAlltoAll_ii, nInh*nInh*sizeof(float));
  
  os_ii.close();
  os_index_ii.close();
  os_postindex_ii.close();
  //os_nonopt_ii.close();
  os_info_ii.close();
  
 // delete[] g;
 // delete[] gAlltoAll;
  delete[] garray;
  delete[] postInd;
  
  /*delete[] gAlltoAll_ee;
  delete[] gAlltoAll_ei;
  delete[] gAlltoAll_ie;
  delete[] gAlltoAll_ii;*/
  
  delete[] garray_ee;
  delete[] garray_ei;
  delete[] garray_ie;
  delete[] garray_ii;

  //delete[] postIndInG;
  /*
  cout << "\nprinting g_ee" << endl;
  printVector(g_ee);
  cout << "\nprinting g_ei" << endl;
  printVector(g_ei);
  cout << "\nprinting g_ie" << endl;
  printVector(g_ie);
  cout << "\nprinting g_ii" << endl;
  printVector(g_ii);

	cout << endl;

  cout << "\nprinting postIndInG_ee" << endl;
  printVector(postIndInG_ee);
  cout << "\nprinting postIndInG_ei" << endl;
  printVector(postIndInG_ei);
  cout << "\nprinting postIndInG_ie" << endl;
  printVector(postIndInG_ie);
  cout << "\nprinting postIndInG_ii" << endl;
  printVector(postIndInG_ii);
  
	cout << endl;

  cout << "\nprinting postInd_ee" << endl;
  printVector(postInd_ee);
  cout << "\nprinting postInd_ei" << endl;
  printVector(postInd_ei);
  cout << "\nprinting postInd_ie" << endl;
  printVector(postInd_ie);
  cout << "\nprinting postInd_ii" << endl;
  printVector(postInd_ii);
  
  cout << "printing precount:" << endl;
  for (unsigned int j=0;j<nN;j++){
    cout <<  precount[j] << " ";
  }
  cout << endl;
  */
  return 0;
}

int printVector(vector<unsigned int>& v){
	for (unsigned int i=0;i<v.size();i++){
		cout << v[i] << " " ;
	}
	cout << endl;
	return 0;
}

int printVector(vector<float>& v){
  for (unsigned int i=0;i<v.size();i++){
		cout << v[i] << " ";
	}
	cout << endl;
	return 0;
}
