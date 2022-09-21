#include <time.h>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include "MCTS.h"
#include "Groups.h"

using namespace std;

#define MAX_VAL 6400
#define MAX_NUM_ITEMS 1000
#define MAX_NUM_SAMPLES 1000
#define MAX_GROUPS 32
#define MAX_ITERS 25

void usage(char *progname) {
  char* usage_str = (char*)
  "     Usage: %s\n"
  "          -m    sets file containing means\n"
  "          -s    sets file containing variances\n"
  "          -t    sets number of cold start runs/users (average these to get performance stats)\n"
  "          -n    sets number of items user is asked to rate\n"
  "          -r    sets number of rollouts\n"
  "          -u    sets file containing user ratings (rather than generating them randomly using means and variances)\n"
  "          -f    sets first item users are asked to rate\n"
  "          -v    enable debug output\n"
  "          -h    prints this message\n";
  printf(usage_str, progname);
}

void readcsv(char* fname, double **vals, int *rows, int *cols, int max_rows) {
  FILE* f = fopen(fname,"r");
  if (f==nullptr) {
    char str[FILENAME_MAX];
    snprintf(str,FILENAME_MAX,"ERROR: Can't open file %s\n",fname);
    perror(str);
    exit(1);
  }
  char buffer[1024*1024];
  *cols=-1; *rows=0;
  while (fgets(buffer, sizeof(buffer), f)) {
    if (buffer[strlen(buffer)-1] != '\n') {
      printf("ERROR: line too long in %s, didn't read to newline\n",fname);
      exit(1);
    }
    int num_items_line=0;
    char *token = strtok(buffer, ",");
    while (token) {
      //printf("%d %d, ",*rows,num_items_line);
      vals[*rows][num_items_line] = atof(token);
      num_items_line++;
      //printf("%g ",n);
      token = strtok(nullptr, ",");
      if (num_items_line == MAX_NUM_ITEMS) {
        printf("ERROR: Too many items >%d in %s\n", MAX_NUM_ITEMS,fname);
        exit(1);
      }
    }
    //printf("\n");
    //printf("read %d items\n",num_items_line);
    if (*cols<0) {
      *cols = num_items_line;
    } else if (*cols != num_items_line) {
      printf("ERROR: inconsistent number of items %d/%d in %s\n",*cols,num_items_line,fname);
      exit(1);
    }
    (*rows)++;
    if (*rows > max_rows) {
      printf("ERROR: Too many rows >%d in %s\n", max_rows,fname);
      exit(1);
    }
  }
}

void read_vals(char* fname, double **vals, int *num_groups, int *num_items) {
  // read items means from file - csv, with one row for each group
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) == nullptr) {
    perror("getcwd() error");
    exit(1);
  }
  char valsfile[PATH_MAX+FILENAME_MAX];
  snprintf(valsfile,FILENAME_MAX,"%s/%s",cwd,fname);
  readcsv(valsfile, vals, num_groups, num_items, MAX_NUM_GROUPS);
  printf("read from %s, num items %d, num_groups %d\n",valsfile,*num_items,*num_groups);
}

void toy_mu_and_sigma(double **mu, double **sigma2, int *num_groups, int *num_items) {
  // toy example
  const int num_groups_0=2;
  *num_groups = num_groups_0;
  *num_items=1000;
  double mu_0[num_groups_0] = {0,1}; //{0,1,2,3,4,5,6,7};
  double sigma2_0[num_groups_0] = {1,1}; //{1,1,1,1,1,1,1,1};
  for (int i=0; i<*num_groups; i++) {
    for (int j=0; j<*num_items; j++) {
      mu[i][j]=mu_0[i];
      sigma2[i][j]=sigma2_0[i];
    }
  }
}

void read_user_ratings_csv(char* fname, double ***ratings, int num_groups, int num_items) {
  char cwd[PATH_MAX];
  string fname_str = string(fname);
  unsigned first = fname_str.find_last_of("_");
  cout << first << endl;
  unsigned last = fname_str.find(".");
  cout << last << endl;
  int nsamples = stoi(fname_str.substr(first + 1, last - first -1));
  cout << nsamples << endl;

  if (getcwd(cwd, sizeof(cwd)) == nullptr) {
    perror("getcwd() error");
    exit(1);
  }
  char rname[PATH_MAX+FILENAME_MAX];
  snprintf(rname,FILENAME_MAX,"%s/%s",cwd,fname);
  int rows, cols;
  double **vals;
  vals = (double**)malloc(MAX_NUM_GROUPS*nsamples*sizeof(double*));
  for (int i=0; i<MAX_NUM_GROUPS*nsamples; i++) {
    vals[i] = (double*)malloc(MAX_NUM_ITEMS*sizeof(double));
  }
  readcsv(rname, vals, &rows, &cols, num_items*nsamples);
  printf("read user ratings: %d %d\n",rows, cols);
  double mean[MAX_NUM_GROUPS][MAX_NUM_ITEMS]={};
  for (int i=0; i<num_groups; i++) {
    for (int j=0; j<nsamples; j++) {
      ratings[i][j] = vals[i*nsamples+j];
      for (int k=0; k<num_items; k++) {
        ratings[i][j][k] = -ratings[i][j][k]; // need to flip sign back to positive
        mean[i][k]+=ratings[i][j][k]/nsamples;
      }
    }
  }
  free(vals);
}

int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  // default parameter settings
  string dataset;
  dataset = "netflix";
  // dataset = "goodreads";
  // dataset = "jester";

  int nyms = 8;

  // default parameter settings
  char *mu_fname=(char*)"mu_netflix8.csv";
  char *sigma2_fname=(char*)"sigma_netflix8.csv";
  string mu_filename = "data/mu_" + dataset + to_string(nyms) + ".csv";
  string sigma_filename = "data/sigma_" + dataset + to_string(nyms) + ".csv";


  //char *ratings_fname=(char*)"test_data_netflix_8_500.csv";
  char *user_ratings_fname=nullptr;
  int max_count=25; // number of items to ask user to rate
  int max_tries=1000;
  int num_rollouts=1;
  int max_lookahead=1; //max_count;
  int max_num_rollouts = max_lookahead-1;
  max_num_rollouts = 0;
  bool use_user_ratings = false;
  int first_item=-1;
  bool use_montecarlo=true;
  //int first_item=199; //206, 113,75, 154
  
  // process command line options
  char c;
  while ((c = (char)getopt(argc, argv,"m:s:t:n:r:f:u:vd:hd:l:c")) != EOF) {
    switch(c) {
      case 'm':
        mu_fname = optarg;
        break;
      case 's':
        sigma2_fname = optarg;
        break;
      case 't':
        max_tries = atoi(optarg);
        break;
      case 'n':
        max_count = atoi(optarg);
        break;
      case 'r':
        num_rollouts = atoi(optarg);
        break;
      case 'l':
        max_lookahead = atoi(optarg);
        max_num_rollouts = max_lookahead-1;
        break;
      case 'u':
        user_ratings_fname = optarg;
        break;
      case 'f':
        first_item = atoi(optarg);
        break;
      case 'v':
        debug = true;
        break;
      case 'c':
        use_montecarlo = false;
        break;
      case 'd':
        dataset = optarg;
        break;
      case 'a':
        nyms = atoi(optarg);
        break;
      case 'h':
        usage(basename(argv[0]));
        exit(0);
      default:
        exit(1);
    }
  }
  printf("settings: max tries=%d, max count %d, num rollouts %d, max_lookahead %d, max_num_rollouts %d first item %d\n", max_tries, max_count,num_rollouts, first_item,max_lookahead,max_num_rollouts);
  
  // read in per-group item rating means and variances
  double **mu, **sigma2;
  mu = (double**)malloc(MAX_NUM_GROUPS*sizeof(double*));
  sigma2 = (double**)malloc(MAX_NUM_GROUPS*sizeof(double*));
  for (int i=0; i<MAX_NUM_GROUPS; i++) {
    mu[i] = (double*)malloc(MAX_NUM_ITEMS*sizeof(double));
    sigma2[i] = (double*)malloc(MAX_NUM_ITEMS*sizeof(double));
  }
  int num_groups,num_items;
  read_vals(&mu_filename[0], mu, &num_groups, &num_items);
  read_vals(&sigma_filename[0], sigma2, &num_groups, &num_items);
  
  Groups groups;
  groups.create(num_groups, mu, sigma2, num_items);
  printf("num groups %d, num_items %d\n",num_groups,num_items);

  // read in pre-recorded user ratings, if specified
  double ***user_ratings=nullptr;
  if (user_ratings_fname) {
    // for testing, load dilina's user ratings data
    user_ratings = (double***)malloc(MAX_NUM_GROUPS*sizeof(double**));
    for (int i=0; i<MAX_NUM_GROUPS; i++) {
      user_ratings[i] = (double**)malloc(MAX_NUM_SAMPLES*sizeof(double*));
    }
    read_user_ratings_csv(user_ratings_fname,user_ratings, num_groups, num_items);
    printf("read user ratings from %s\n",user_ratings_fname);
  }
  
  const double time_limit = 0.0; // in milliseconds.  not used.
  const int max_disp_count=25; // truncate lengthy output after this many lines
  
  double rewards[MAX_NUM_GROUPS]={};
  auto overall_start = chrono::steady_clock::now();
  int disp_count=0;
  // this magic openmp pragma parallelises the for loop, we keep separate state within loop
  // so copies can be run without generating races.
  // to install openmp use "brew install llvm omp"  (need to install llvm as default clang
  // install doesn't support openmp, sigh.



  #pragma omp parallel for
  for (int user_group=0; user_group<num_groups; user_group++) {
    rewards[user_group]=0;
    MonteCarloTree tree; // by keeping separate tree instances here we can parallelise loop
    for (int tries=0; tries<max_tries; tries++){
      if (disp_count<max_disp_count) {
        printf("**try %d\n",tries);
      }
      int used_items[MAX_NUM_ITEMS] = {};
      int used_items_list[MAX_NUM_ITEMS] = {};
      int num_used_items=0;
      double ratings[MAX_NUM_ITEMS] = {};
      double probs[MAX_NUM_GROUPS];
      for (int g=0; g<num_groups; g++){
        probs[g]=1.0/num_groups;
      }
      
      if (first_item>=0) {
        // use pre-defined first item user is asked to rate
        used_items[first_item]=1; // record that this item has now been used
        used_items_list[num_used_items]=first_item;
        if (use_user_ratings) {
          ratings[num_used_items] = user_ratings[user_group][tries][first_item];
        } else {
          ratings[num_used_items] = groups.rating(user_group,first_item);
        }
        num_used_items++;
        groups.calc_group_probs(used_items_list, ratings, num_used_items, probs);
      }
      
      while (num_used_items<max_count) {
        tree.reset();
        // hacky kind of heuristic for number of runs of mcts to use ...
        // run out mem on my laptp if make prefactor larger than about 7.

        int sim_k = 1;
        int simulation_counts=int(sim_k*num_items*(1.25+(max_count-num_used_items)*(max_count-num_used_items)));

        if (!use_montecarlo) {
          simulation_counts=num_items;
        }
        /*for (int g=0; g<num_groups; g++){
         printf("%g ",probs[g]);
         }
         printf("\n");*/
        int count_sim = 0;
        auto start = chrono::steady_clock::now();
        double diff_time=0.0;
        while ((count_sim < simulation_counts)||(diff_time < time_limit )) {
          //printf("while %d\n",count_sim);
          tree.run(&groups, probs, used_items, used_items_list, ratings, num_used_items, max_count, num_rollouts, max_lookahead, max_num_rollouts,use_montecarlo);

          count_sim++;
          diff_time = chrono::duration<double, milli>(chrono::steady_clock::now() - start).count();
        }
        //std::vector<int> path={}; tree.print_tree(tree.root, path);
        if (child_lowestN(tree.root)==0) {
          printf("WARNING: unvisited child nodes, increase simulation_counts from %d.\n", simulation_counts);
        }
        int next_item = best_child2(tree.root);
        used_items[next_item]=1; // record that this item has now been used
        used_items_list[num_used_items]=next_item;
        //user_group=1;
        if (user_ratings) {
          // use pre-recorded user ratings
          ratings[num_used_items] = user_ratings[user_group][tries][next_item];
        } else {
          // generate a random rating with specified mean and variance
          ratings[num_used_items] = groups.rating(user_group,next_item);
        }
        if (disp_count<max_disp_count) {
          // stop display once gets larger
          printf("%d %d %g, time %gms/num runs %d\n",num_used_items,next_item,ratings[num_used_items],diff_time, count_sim);
          disp_count++;
        }
        num_used_items++;
        groups.calc_group_probs(used_items_list, ratings, num_used_items, probs);
      }
      /*for (int g=0; g<num_groups; g++){
       printf("%g ",probs[g]);
       }
       printf("\n");*/
      int g = groups.estimated_group(used_items_list, ratings, num_used_items);
      //printf("g=%d\n",g);
      if (g==user_group) {
        rewards[user_group]++;
      }
      
      // for(int i = 0; i < num_used_items; i++){
        
      //   int g = groups.estimated_group(used_items_list, ratings, i+1);
      //   if (g==user_group) {
      //     group_acc[user_group][i] += 1;
      //   }
      // }

      
    }
    rewards[user_group] = rewards[user_group]*1.0/max_tries;
    printf("group %d success rate %g\n", user_group, rewards[user_group]);
  }
  

  const int dir_err = mkdir("output", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (-1 == dir_err){
      printf("Directory already exists OR Error creating directory!n");
  }
  
  printf("time taken %g sec\n",chrono::duration<double, milli>(chrono::steady_clock::now() - overall_start).count()/1000.0);

  printf("acc per iter:\n");
  for (int i=0; i<max_count; i++) {
    printf("%4d ",i+1);
  }
  printf("\n");


  printf("group/success rate:\n");
  for (int i=0; i<num_groups; i++) {
    printf("%4d ",i);
  }
  printf("\n");
  double mean=0.0;
  for (int i=0; i<num_groups; i++) {
    printf("%4g ",rewards[i]);
    mean+=rewards[i];
  }
  printf("\nmean=%g\n",mean/num_groups);

}
