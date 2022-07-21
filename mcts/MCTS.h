#pragma once

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <cstdlib>
#include <chrono>
#include <cstring>

#define USE_GSL 1
#ifdef USE_GSL
// GNU Scientific Library - install using "brew install gsl"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#else
#include <random>
#endif

#include "Groups.h"
#include "TreeNode.h"
#include "utils.h"

class MonteCarloTree {
public:
  TreeNode* root=nullptr;
  TreeNodeMem treeMem;
  MonteCarloTree() : root(nullptr) {}
#ifdef USE_GSL
  gsl_rng *gen;
  inline double uniform_rnd() {
    return gsl_rng_uniform(gen);
  }
#else
  std::random_device rd{};
  std::mt19937 gen{rd()}; // mersenne twister, faster?
  //std::minstd_rand0 gen{rd()}; // faster?
  std::uniform_real_distribution<double> uniform(0.0,1.0);
  inline double uniform_rnd() {
    return uniform(gen);
  }
#endif

  TreeNode* UCB (TreeNode* n)  {		
    // find a node with highest score
    double max_score = -1;
    //TreeNode* bestNode = nullptr;
#define MAX_BESTNODES 100
    TreeNode* bestNodes[MAX_BESTNODES]; int num_bestNodes=0;
    // var=0.25 for a bernoulli RV
    double logN=0.25*log(n->N*1.0); // move outside loop, compiler seems to not spot this optimisation
    for (int i = 0; i < n->child_size; ++i) {
      TreeNode* childNode = n->child[i];
      
      // TO DO: should choose randomly amongst unvisited nodes
      if (childNode->N==0) {
        // an unvisited node, let's visit it.
        return childNode;
      }
      
      double Q = childNode->Q/childNode->N;
      //double var = childNode->Q2/childNode->N - Q*Q;
      double explore = sqrt( logN/childNode->N );
      double score= Q + explore;
      
      if (score > max_score) {
        max_score = score;
        bestNodes[0]=childNode;
        num_bestNodes=1;
        //bestNode = childNode;
      } else if ((score > max_score-1.0e-2)&&(num_bestNodes<MAX_BESTNODES)) {
        // keep rough track of nodes with scores close to the max, we'll treat these as ties.
        bestNodes[num_bestNodes]=childNode;
        num_bestNodes++;
      }
    }	
    
    
     /*
     // more exact book-keeping of ties, but slower.
     num_bestNodes=0;
     for (int i = 0; i < n->child_size; ++i) {
       TreeNode* childNode = n->child[i];
       double exploit = childNode->Q*1.0/childNode->N;
       double explore = sqrt( 2*log( n->N*1.0)/childNode->N );
       double score= exploit + explore ;
       if ( (score-max_score)<1.0e-2 && (score-max_score)>-1.0e-2) {
          bestNodes[num_bestNodes]=childNode;
          num_bestNodes++;
          if (num_bestNodes>=MAX_BESTNODES) break;
       }
     }
     */
    
    // break ties randomly
    int rnd = (int) ( uniform_rnd() * (num_bestNodes-1) + 0.5);
    return bestNodes[rnd];
  }
  
  int select(TreeNode** path) {
    int num_path=0;
    TreeNode* current = root;
    path[num_path] = current;
    num_path++;
    DEBUG_PRINT("select %d/%d\n",current->item,current->child_size);
    while (current->child_size != 0) {
      // move to best child node
      current = UCB(current);
      path[num_path] = current;
      num_path++;
      DEBUG_PRINT("select added %d\n",current->item);
    }
    return num_path;
  }
  
  int rollout(Groups *groups, int *used_items, int num_path_items, int max_count, int* rollout_items) {
    // random rollout
    DEBUG_PRINT("rollout, num_path_items %d max_count %d num_items %d\n", num_path_items, max_count,groups->num_items);
    int tmp_used_items[groups->num_items];
    memcpy(tmp_used_items,used_items,groups->num_items*sizeof(int));
    /*for (int i=0; i<groups->num_items;i++){
     if (tmp_used_items[i]!=0) {
     printf("%d/%d ",i,tmp_used_items[i]);
     }
     }
     printf("\n");*/
    int num_rollout_items=0;
    for (int i=num_path_items; i<max_count; i++) {
      // choose random item, not already selected
      int item = (int) ( uniform_rnd() * (groups->num_items-1) + 0.5); // round
      DEBUG_PRINT("rollout item %d\n",item);
      int old_item = item;
      while (tmp_used_items[item]) {
        item = (int) ( uniform_rnd() * (groups->num_items-1) + 0.5); // round
        //printf("%d ",item);
      }
      if (item != old_item) {
        DEBUG_PRINT("rollout replaced dup item %d with %d\n",old_item,item);
      }
      rollout_items[num_rollout_items]=item;
      num_rollout_items++;
      tmp_used_items[item]=1;
      DEBUG_PRINT("rollout added item %d\n",item);
    }
    DEBUG_PRINT("rollout done, num_rollout_items %d: ", num_rollout_items); print_items(rollout_items,num_rollout_items);
    return num_rollout_items;
  }
  
  void backpropagate(double result, TreeNode** path, int num_path) {
    for (int i=0; i<num_path; i++) {
      path[i]->N++;
      path[i]->Q+=result;
      //path[i]->Q2+=result*result;
    }
    print_path(path, num_path);
  }
  
  int get_pathitems(TreeNode** path, int num_path, int *items) {
    int num_items=0;
    for (int i=0; i<num_path; i++) {
      if (path[i]->item >=0 ) { // root node has item=-1, exclude this
        items[num_items]=path[i]->item;
        num_items++;
      }
    }
    return num_items;
  }
  
  void print_path(TreeNode** path, int num_path) {
    for (int i=0; i<num_path; i++) {
      DEBUG_PRINT("item %d, Q=%g, N=%d\n",path[i]->item, path[i]->Q, path[i]->N);
      //printf("item %d, Q=%g, N=%d\n",path[i]->item, path[i]->Q, path[i]->N);
    }
  }
  
  void print_tree(TreeNode* node, std::vector<int> path) {
    for (auto &p : path) {
      printf("%d:",p);
    }
    printf("%d (Q,N) %g/%d\n",node->item,node->Q,node->N);
    std::vector<int> path2(path);
    path2.push_back(node->item);
    for (int i=0; i<node->child_size;i++) {
      print_tree(node->child[i], path2);
    }
  }
  
  void print_itemarray(int* items, int num_items) {
    for (int i=0; i<num_items;i++){
      if (items[i]!=0) {
        DEBUG_PRINT("%d/%d ",i,items[i]);
      }
    }
    DEBUG_PRINT("\n");
  }
  
  void run(Groups *groups, double* probs, int* used_items, int* used_items_list, double* used_ratings, int num_used_items, int max_count, int num_rollouts, int max_lookahead, int max_num_rollout_items, bool use_montecarlo) {
    //auto start = std::chrono::steady_clock::now();
    //DEBUG_PRINT("run num_items %d, num_used_items %d:\n",groups->num_items,num_used_items); print_itemarray(used_items,groups->num_items);
    double cumsum_probs[MAX_NUM_GROUPS];
    cumsum_probs[0]=probs[0];
    for (int g=1; g<groups->num_groups; g++){
      cumsum_probs[g]=cumsum_probs[g-1]+probs[g];
      //printf("%g ",cumsum_probs[g]);
    }
    //printf("\n");
    
    TreeNode* path[max_count];
    int num_path=0;
    num_path = select(path);
    //DEBUG_PRINT("selected path\n"); print_path(path,num_path);
    TreeNode *leaf_node = path[num_path-1];
    if ((leaf_node->item<0) || ((leaf_node->N>0) && (leaf_node->child_size==0) && (num_path<max_lookahead+1)) ) { // already visited, now expand
      expand(leaf_node,path,num_path,groups->num_items,used_items);
      if (leaf_node->child_size > 0) {
        leaf_node = UCB(leaf_node);
        path[num_path]=leaf_node; num_path++;
      } 
      //DEBUG_PRINT("expanded, num_path=%d\n",num_path); print_path(path,num_path);
    }
    int path_items[max_count], num_path_items=0;
    num_path_items = get_pathitems(path,num_path,path_items);
    //DEBUG_PRINT("path %d: ",num_path_items); print_items(path_items,num_path_items);
    //DEBUG_PRINT("got path items\n");
    double reward=0.0;
    if (use_montecarlo) {
      // sample ratings and estimate average
      int tmp_used_items[groups->num_items];
      memcpy(tmp_used_items, used_items, groups->num_items*sizeof(int));
      for (int i=0; i<num_path_items; i++) { //get_pathitems() already excludes root node
  #ifdef DEBUG_MEM
        if (path_items[i]<0 || path_items[i]>MAX_BRANCHING-1) {
          printf("ERROR: In run() tmp_used_items item %d is out of range",path_items[i]);
          exit(1);
        }
  #endif
        tmp_used_items[path_items[i]]=1;
      }
      // a small optimisation, take repeated calc outside rollout for loop ...
      double init_err[MAX_NUM_GROUPS]={};
      groups->init_reward_err(used_items_list, used_ratings, num_used_items, init_err);
      
      for (int i=0; i<num_rollouts*groups->num_groups; i++) {
        //DEBUG_PRINT("rollout %d\n",i);
        int rollout_items[max_count], num_rollout_items=0;
        if (max_num_rollout_items>0) {
          num_rollout_items = rollout(groups, tmp_used_items, num_used_items+num_path_items, max_count, rollout_items);

          // int num_total_used = num_used_items+num_path_items;
          // int num_roll_items = fmax(5 - num_total_used, max_num_rollout_items);
          // if (num_rollout_items > num_roll_items) {
          //   num_rollout_items = num_roll_items;
          // }

          if (num_rollout_items > max_num_rollout_items) {
            num_rollout_items = max_num_rollout_items;
          }
        }
        // we don't know the true user group, so calc rollout for all groups and take average reward
        // -- weight groups non-uniformly for now, but could change that?
        double r=uniform_rnd();
        int g;
        for (g=0; g<groups->num_groups; g++){
          if (r <= cumsum_probs[g]) break;
        }
        reward += groups->reward(g, path_items, num_path_items, rollout_items, num_rollout_items, init_err);
        // reward += groups->discounted_reward(g, path_items, num_path_items, rollout_items, num_rollout_items, init_err);
        // reward += groups->discounted_reward(g, path_items, num_path_items, used_items_list, num_used_items, rollout_items, num_rollout_items);
      }
      reward = reward/(num_rollouts*groups->num_groups);
    } else {
      // use mean rating instead of sampling.
      int tmp_used_items_list[num_used_items+1];
      memcpy(tmp_used_items_list, used_items_list, num_used_items*sizeof(int));
      tmp_used_items_list[num_used_items]=path_items[0];
      double tmp_ratings[num_used_items+1];
      memcpy(tmp_ratings,used_ratings,num_used_items*sizeof(double));
      for (int g=0; g<groups->num_groups; g++) {
        // one-step ahead only for now i.e. num_path_items=1 and no rollout items
        tmp_ratings[num_used_items]=groups->mean_rating(g, path_items[0]);
        double tmp_groupprobs[MAX_NUM_GROUPS];
        groups->calc_group_probs(tmp_used_items_list, tmp_ratings, num_used_items+1, tmp_groupprobs);
        reward += probs[g]*tmp_groupprobs[g];
      }
    }
    //printf("time %g/%g\n",std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start2).count(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    //DEBUG_PRINT("reward %g\n",reward);
    backpropagate(reward, path, num_path);
    //DEBUG_PRINT("backpropagated\n");
    
  }
  
  void reset() {
    if (root!=nullptr) {
      // throw away old tree
      free_allTreeNodes(&treeMem);
      root=nullptr;
    }
    root = alloc_TreeNode(&treeMem);
    root->item=-1; // mark node as root
    root->child_size=0;
    root->N=0; root->Q=0; //root->Q2=0;
    //srand((unsigned int)time(NULL));
#ifdef USE_GSL
    const gsl_rng_type *T;
    gsl_rng_env_setup();
    //T = gsl_rng_default;
    T = gsl_rng_taus2; // slightly faster than mersenne twister
    gen = gsl_rng_alloc(T);
    gsl_rng_set(gen, (unsigned long)time(NULL));
#endif
    
  }
  
};
