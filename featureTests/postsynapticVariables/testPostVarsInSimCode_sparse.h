
#ifndef TESTPREVARSINSIMCODE_H
#define TESTPREVARSINSIMCODE_H

#define DT 0.1f
#define TOTAL_TIME 20.0f
#define REPORT_TIME 1.0f

class postVarsInSimCode_sparse
{

public:
  postVarsInSimCode_sparse();
  ~postVarsInSimCode_sparse();
  void init_synapses();
  void init_neurons();
  void run(int);

  float **theW;
};

#endif // SYNDELAYSIM_HPP
