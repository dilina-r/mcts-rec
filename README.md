# mcts-rec

## Introduction

The C++ implementation for the publication "Fast And Accurate User Cold-Start Learning Using Monte Carlo Tree Search". The paper can be found [here](https://dl.acm.org/doi/10.1145/3523227.3546786).


### How to set up

Install GNU Scientific Library [(GSL)](https://www.gnu.org/software/gsl/).

Installing `gsl` in macOS
```
brew install gsl
```


### Preprocessed Data

The item distribution data (mean and variance) of the clusters for each dataset can be found [here](https://www.dropbox.com/sh/l0d6rynq7lte40i/AAB9OiUyMcuT_N8-BbMPakooa?dl=0).

The test data can be found [here](https://www.dropbox.com/sh/iplhtgczu5tdmpo/AABMcIeLE-kM_Sk1MdoJMGD3a?dl=0).


## How to run

```
make
./bin/mcts -t <samples per group> -n <num recommendations>
```
