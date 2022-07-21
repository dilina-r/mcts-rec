#pragma once

#include <vector>
#include <chrono>
#include "utils.h"

#include <random>
std::random_device rd{};
std::mt19937 gen{rd()}; // mersenne twister, faster?
std::uniform_real_distribution<double> uniform(0.0,1.0);

// we trade off memory usage for greater speed by preallocating child array.
// this also limits the max number of items that can be conisdered
#define MAX_BRANCHING 1500

struct TreeNode {
  TreeNode* child[MAX_BRANCHING];
  int child_size;
  int item;
  int N;
  double Q;
  //double Q2;
  void* mem;
};

// do our own memory management
struct TreeNodeMem {
  std::vector<TreeNode*> availTreeNodes={};
  int list_posn=0, node_posn=0;
  int numTreeNodesallocated=0;
  int numTreeNodesreused=0;
};
TreeNode* alloc_TreeNode(TreeNodeMem* mem) {
  if (mem->availTreeNodes.size()==0) {
    // allocate a block of new tree nodes since expect repeated calls from expand()
    TreeNode* newnodes = (TreeNode*)malloc(sizeof(TreeNode)*MAX_BRANCHING);
    mem->availTreeNodes.push_back(newnodes);
    mem->list_posn=0;
    mem->node_posn=0;
    mem->numTreeNodesallocated+=MAX_BRANCHING;
  } else {
    mem->numTreeNodesreused++;
  }
  TreeNode* newnode = &(mem->availTreeNodes[mem->list_posn][mem->node_posn]);
  newnode->mem = mem;
  mem->node_posn++;
  if (mem->node_posn==MAX_BRANCHING) {
    mem->node_posn=0;
    mem->list_posn++;
    if (mem->list_posn == (int)mem->availTreeNodes.size()){
      TreeNode* newnodes = (TreeNode*)malloc(sizeof(TreeNode)*MAX_BRANCHING);
      mem->availTreeNodes.push_back(newnodes);
      mem->numTreeNodesallocated+=MAX_BRANCHING;
    }
  }
  return newnode;
}
void free_allTreeNodes(TreeNodeMem* mem) {
  // move all the allocated nodes back onto the free list
  mem->list_posn=0; mem->node_posn=0;
  DEBUG_PRINT("num tree nodes allocated/reused %d/%d\n",mem->numTreeNodesallocated,mem->numTreeNodesreused);
}


void print_TreeNode(TreeNode* node) {
  printf("node (item,Q,N) %d:%g/%d, children:",node->item,node->Q,node->N);
  for (int i=0; i<node->child_size; i++) {
    printf("%d:%g/%d ",node->child[i]->item,node->child[i]->Q,node->child[i]->N);
  }
  printf("\n");
}

void expand(TreeNode* node, TreeNode** path, int num_path, int num_items,  int* used_items) {
  DEBUG_PRINT("expand num_items %d, num_path %d\n",num_items,num_path);
  if (num_items>MAX_BRANCHING) {
    printf("ERROR: number of items %d > tree MAX_BRANCHING %d\n!",num_items,MAX_BRANCHING);
    exit(1); // this is fatal
  }
  int path_items[MAX_BRANCHING]={};
  int unused_list[MAX_BRANCHING]={}, num_list=0;
  for (int i=1; i<num_path; i++) { // first node of path is root, it has item -1
#ifdef DEBUG_MEM
    if (path[i]->item<0 || path[i]->item>MAX_BRANCHING-1) {
      printf("ERROR: In expand() item %d is out of range",path[i]->item);
      exit(1);
    }
#endif
    path_items[path[i]->item]=1;
  }
  for (int m=0; m<num_items; m++) {
    if ((!used_items[m]) && (!path_items[m])) {
      // item m not shown to user and not in a parent node
      unused_list[num_list]=m;
      num_list++;
    };
  }
  //printf("expand %d %d %d",root->num_items, path->size(), node->item);
  DEBUG_PRINT("expanded unused items %d: ",num_list); print_items(unused_list, num_list);
  
  if (num_list == 0) {
    return;
  }
  
  node->child_size = num_list;
  
  //#pragma omp parallel for
  for (int i=0; i<node->child_size; i++) {
    //#pragma omp critical
    {
      node->child[i] = alloc_TreeNode((TreeNodeMem*)node->mem);
    }
    node->child[i]->item = unused_list[i];
    node->child[i]->Q=0;
    //node->child[i]->Q2=0;
    node->child[i]->N=0;
    node->child[i]->child_size=0;
  }
}

int child_lowestN(TreeNode* node) {
  if (node->child_size == 0) {
    return -1;
  }
  int lowestN=node->child[0]->N;
  for (int i = 1 ; i < node->child_size; ++i) {
    if (node->child[i]->N < lowestN) {
      lowestN = node->child[i]->N;
    }
  }
  return lowestN;
}

int best_child(TreeNode* node) {
  int most_visited = -1;
  double highest_score = -1;
  int highest_item = -1;
  double logN=2*log(node->N*1.0);
  //int best_item = -1;
  
  if (node->child_size == 0) {
    return -1;
  }
  for (int i = 0 ; i < node->child_size; ++i) {
    if (node->child[i]->N > most_visited) {
      most_visited = node->child[i]->N;
      //best_item = node->child[i]->item;
    }
    double score = node->child[i]->Q/node->child[i]->N + sqrt( logN/node->child[i]->N );
    if (score > highest_score) {
      highest_score = score;
      highest_item = node->child[i]->item;
    }
  }
// return highest_item;
  // break ties randomly
#define MAX_BESTITEMS 100
// printf("score:%g, item:%d\n", highest_score, highest_item);
  int best_items[MAX_BESTITEMS]; int num_bestitems=0;
  for (int i = 0 ; i < node->child_size; ++i) {
    if ((node->child[i]->N == most_visited)) {
      double score = node->child[i]->Q/node->child[i]->N + sqrt( logN/node->child[i]->N );
      if (score < 0.95*highest_score) {
        // should probably increase number of runs if this happens
        //printf("WARNING: most visited node score %g does not have highest score %g\n",score, highest_score);
      }
      best_items[num_bestitems]=node->child[i]->item;
      // printf("score:%g, item:%d\n", score, best_items[num_bestitems]);
      num_bestitems++;
      if (num_bestitems==MAX_BESTITEMS) {
        break;
      }
    }
  }
  int rnd = (int)(uniform(gen)*(num_bestitems-1)+0.5);
  // exit(0);
  return best_items[rnd];
}



int best_child2(TreeNode* node) {
  int most_visited = -1;
  double highest_score = -1;
  int highest_item = -1;
  double logN=2*log(node->N*1.0);
  //int best_item = -1;
  
  if (node->child_size == 0) {
    return -1;
  }
  for (int i = 0 ; i < node->child_size; ++i) {
    if (node->child[i]->N > most_visited) {
      most_visited = node->child[i]->N;
      //best_item = node->child[i]->item;
    }
    // double score = node->child[i]->Q/node->child[i]->N + sqrt( logN/node->child[i]->N );
    double score = node->child[i]->Q/node->child[i]->N;
    if (score > highest_score) {
      highest_score = score;
      highest_item = node->child[i]->item;
    }
  }
// return highest_item;
  // break ties randomly
#define MAX_BESTITEMS 100
// printf("highest score:%g, item:%d, min score: %g\n", highest_score, highest_item, 0.95*highest_score);
  int best_items[MAX_BESTITEMS]; int num_bestitems=0;
  for (int i = 0 ; i < node->child_size; ++i) {
    double score = node->child[i]->Q/node->child[i]->N;
    if (score >= 0.95*highest_score) {
      best_items[num_bestitems]=node->child[i]->item;
      // printf("score:%g, item:%d\n", score, best_items[num_bestitems]);
      num_bestitems++;
      if (num_bestitems==MAX_BESTITEMS) {
        break;
      }
    }
  }
  int rnd = (int)(uniform(gen)*(num_bestitems-1)+0.5);
  // exit(0);
  return best_items[rnd];
}
