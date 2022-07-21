#pragma once

#define USE_GSL 1
#ifdef USE_GSL
// GNU Scientific Library - install using "brew install gsl"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#else
#include <random>
#endif
#include <time.h>
#include "utils.h"

#define MAX_NUM_GROUPS 128

class Groups {
public:
  int num_groups;
  int num_items;
  double **mu;
  double **sigma2;
#ifdef USE_GSL
  gsl_rng *gen;
#else
  std::random_device rd{};
  std::mt19937 gen{rd()}; // mersenne twister, faster?
  //std::minstd_rand0 gen{rd()}; // faster?
  std::normal_distribution<> gaussian {0.0,1.0};
#endif
  
  void create(int num_groups, double **mu, double **sigma2, int num_items) {
    if (num_groups>MAX_NUM_GROUPS) {
      printf("ERROR: number of groups %d > MAX_NUM_GROUPS %d!\n",num_groups,MAX_NUM_GROUPS);
      exit(1); // this is fatal
    }
    this->num_groups = num_groups;
    this->mu = mu;
    this->sigma2 = sigma2;
    this->num_items=num_items;
    /*for (int g=0; g<num_groups; g++) {
     printf("g=%d, mu=%g, sigma2=%g\n",g,this->mu[g],this->sigma2[g]);
     }*/
    // initialise random number generator
#ifdef USE_GSL
    const gsl_rng_type *T;
    gsl_rng_env_setup();
    //T = gsl_rng_default;
    T = gsl_rng_taus2; // slightly faster than mersenne twister
    gen = gsl_rng_alloc(T);
    gsl_rng_set(gen, (unsigned long)time(NULL));
#endif
  }
  
  inline double gaussian(double sigma) {
    // this is the code hot spot, its the main bottleneck in the whole programme
    // gsl ziggurat random number generator seems quite a bit faster than standard c++ one.
#ifdef USE_GSL
    return gsl_ran_gaussian_ziggurat(gen, sigma);
#else
    return gaussian(gen)*sigma;
#endif
  }
  
  inline double mean_rating(int group, int item) {
    return mu[group][item];
  }
  
  inline double rating(int group, int item) {
    return mu[group][item] + gaussian(sqrt(sigma2[group][item]));
  }
  
  void calc_group_probs(int* items, double* ratings, int num_items, double *probs) {
    double sum[MAX_NUM_GROUPS]={}, prod[MAX_NUM_GROUPS];
    for (int g=0; g<num_groups; g++) {
      prod[g]=1.0;
    }
    for (int i=0; i<num_items; i++) {
      for (int g=0; g<num_groups; g++) {
        sum[g] += (ratings[i]-mu[g][items[i]])*(ratings[i]-mu[g][items[i]])/sigma2[g][items[i]];
        prod[g] *= sqrt(sigma2[g][items[i]]);
      }
    }
    double sum_prob=0;
    for (int g=0; g<num_groups; g++) {
      probs[g] = exp(-sum[g]/2.0)/prod[g];
      sum_prob += probs[g];
    }
    for (int g=0; g<num_groups; g++) {
      probs[g] = probs[g]/sum_prob;
    }
  }
  
  
  inline int estimated_group(int *items, double *ratings, int num_items) {
    double probs[MAX_NUM_GROUPS];
    calc_group_probs(items, ratings, num_items, probs);
    int best_group=0;
    double max=probs[0];
    for (int g=1; g<num_groups; g++) {
      if (probs[g]>max) {
        max=probs[g];
        best_group=g;
      }
    }
    
    /*double err[MAX_NUM_GROUPS]={};
     for (int i=0; i<num_items; i++) {
     double r = ratings[i];
     for (int g=0; g<num_groups; g++) {
     #ifdef DEBUG_MEM
     if (items[i]<0 || items[i]>this->num_items-1) {
     printf("ERROR: In estimated_group() items %d is out of range",items[i]);
     exit(1);
     }
     #endif
     //printf("g=%d, mu=%g, sigma=%g, val=%g, r=%g\n",g,mu[g],sigma2[g],pow(r-mu[g],2)/sigma2[g],r);
     err[g] += (r-mu[g][items[i]])*(r-mu[g][items[i]])/sigma2[g][items[i]];
     }
     }
     int best_group=0;
     double min_err=err[0];
     for (int g=1; g<num_groups; g++) {
     if (err[g]<min_err) {
     min_err=err[g];
     best_group=g;
     }
     }*/
    return best_group;
  }

  inline void  init_reward_err(int* used_items, double* ratings, int num_used_items, double* err) {
    for (int i=0; i<num_used_items; i++) {
      double r = ratings[i];
      for (int g=0; g<num_groups; g++) {
        err[g] += (r-mu[g][used_items[i]])*(r-mu[g][used_items[i]])/sigma2[g][used_items[i]];
      }
    }
  }

  inline void  init_reward_err2(int usergroup, int* used_items, double* ratings, int num_used_items, double* err) {
    for (int i=0; i<num_used_items; i++) {
      double r = ratings[i];
      // double r = rating(usergroup, used_items[i]);
      for (int g=0; g<num_groups; g++) {
        err[g] += (r-mu[g][used_items[i]])*(r-mu[g][used_items[i]])/sigma2[g][used_items[i]];
      }
    }
  }
    
  inline int reward(int user_group, int *items, int num_items, int *rollout_items, int num_rollout_items, double* init_err) {
    // here we make a fresh draw of ratings for items not yet rated by user
    DEBUG_PRINT("reward num_groups %d\n",num_groups);
    
    double err[MAX_NUM_GROUPS];
    // copy initial value of err array 
    memcpy(err,init_err,MAX_NUM_GROUPS*sizeof(double));
    
    for (int i=0; i<num_items; i++) {
#ifdef DEBUG_MEM
      if (items[i]<0 || items[i]>this->num_items-1) {
        printf("ERROR: In reward() items %d is out of range",items[i]);
        exit(1);
      }
#endif
      double r = rating(user_group, items[i]);
      for (int g=0; g<num_groups; g++) {
        //printf("g=%d, mu=%g, sigma=%g, val=%g\n",g,mu[g],sigma2[g],pow(r-mu[g],2)/sigma2[g]);
        err[g] += (r-mu[g][items[i]])*(r-mu[g][items[i]])/sigma2[g][items[i]];
      }
    }
    for (int i=0; i<num_rollout_items; i++) {
#ifdef DEBUG_MEM
      if (rollout_items[i]<0 || rollout_items[i]>this->num_items-1) {
        printf("ERROR: In reward() rollout_items %d is out of range",rollout_items[i]);
        exit(1);
      }
#endif
      double r = rating(user_group, rollout_items[i]);
      for (int g=0; g<num_groups; g++) {
        //printf("g=%d, mu=%g, sigma=%g, val=%g\n",g,mu[g],sigma2[g],pow(r-mu[g],2)/sigma2[g]);
        err[g] += (r-mu[g][rollout_items[i]])*(r-mu[g][rollout_items[i]])/sigma2[g][rollout_items[i]];
      }
    }
    DEBUG_PRINT("reward err[] done\n");
    for (int g=0; g<num_groups; g++) {
      DEBUG_PRINT("%g ",err[g]);
    }
    DEBUG_PRINT("\n");
    
    int best_group=0;
    double min_err=err[0];
    for (int g=1; g<num_groups; g++) {
      if (err[g]<min_err) {
        min_err=err[g];
        best_group=g;
      }
    }
    
    DEBUG_PRINT("best group %d/%d\n", best_group,user_group);
    if (best_group == user_group) {
      return 1; // predicted the correct user group
    } else {
      return 0;
    }
  }

  inline int discounted_reward(int user_group, int *items, int num_items, int *rollout_items, int num_rollout_items, double* init_err) {
    // here we make a fresh draw of ratings for items not yet rated by user
    DEBUG_PRINT("reward num_groups %d\n",num_groups);
    
    double err[MAX_NUM_GROUPS];
    // copy initial value of err array 
    memcpy(err,init_err,MAX_NUM_GROUPS*sizeof(double));
    
    for (int i=0; i<num_items; i++) {
#ifdef DEBUG_MEM
      if (items[i]<0 || items[i]>this->num_items-1) {
        printf("ERROR: In reward() items %d is out of range",items[i]);
        exit(1);
      }
#endif
      double r = rating(user_group, items[i]);
      for (int g=0; g<num_groups; g++) {
        //printf("g=%d, mu=%g, sigma=%g, val=%g\n",g,mu[g],sigma2[g],pow(r-mu[g],2)/sigma2[g]);
        err[g] += (r-mu[g][items[i]])*(r-mu[g][items[i]])/sigma2[g][items[i]];
      }
    }

    int best_group=0;
    double min_err=err[0];
    for (int g=1; g<num_groups; g++) {
      if (err[g]<min_err) {
        min_err=err[g];
        best_group=g;
      }
    }
    if (best_group == user_group) {
      return 1; // predicted the correct user group
    } 


    for (int i=0; i<num_rollout_items; i++) {
#ifdef DEBUG_MEM
      if (rollout_items[i]<0 || rollout_items[i]>this->num_items-1) {
        printf("ERROR: In reward() rollout_items %d is out of range",rollout_items[i]);
        exit(1);
      }
#endif
      double r = rating(user_group, rollout_items[i]);
      for (int g=0; g<num_groups; g++) {
        //printf("g=%d, mu=%g, sigma=%g, val=%g\n",g,mu[g],sigma2[g],pow(r-mu[g],2)/sigma2[g]);
        err[g] += (r-mu[g][rollout_items[i]])*(r-mu[g][rollout_items[i]])/sigma2[g][rollout_items[i]];
      }
    }
    DEBUG_PRINT("reward err[] done\n");
    for (int g=0; g<num_groups; g++) {
      DEBUG_PRINT("%g ",err[g]);
    }
    DEBUG_PRINT("\n");
    
    best_group=0;
    min_err=err[0];
    for (int g=1; g<num_groups; g++) {
      if (err[g]<min_err) {
        min_err=err[g];
        best_group=g;
      }
    }
    
    DEBUG_PRINT("best group %d/%d\n", best_group,user_group);
    if (best_group == user_group) {
      return 0.5; // predicted the correct user group
    } else {
      return 0;
    }
  }


};
