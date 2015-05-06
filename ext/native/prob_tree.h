#ifndef PROB_TREE_H
#define PROB_TREE_H

#include <pthread.h>
#include <ruby.h>

#define MAX_CARDINALITY 100000
#define PUTS(OBJ) (rb_funcall(rb_cObject, rb_intern("puts"), 1, OBJ))
#define SYM(STR) (ID2SYM(rb_intern(STR)))

//A probability of a thing
typedef struct prob_node {
  long long successes;
  double probspace;
  int attempts;
} pnode_t;

//A C representation the hashed valued for the probabilities
typedef struct prob_dist_item_node_struct {
  int index;
  int reward;
  double prob;
} prob_dist_item_node_t;

//Node for a single probability distribution.
typedef struct prob_dist_struct {
  int num_prob;
  prob_dist_item_node_t** probs;
} prob_dist_t;

//The tree!
typedef struct prob_tree {
  pnode_t** current_ply;
  pnode_t** next_ply;
  long long goal;
  long long cardinality;
  int current_prob_dist;
  int depth;
  
  //Not sure if this belongs to the tree...
  prob_dist_t** prob_dists; 
} ptree_t;

//This is a wrapper struct. It may be replaced by a reference to the whole tree.
//TODO: See about passing a reference to the whole tree.
typedef struct ptree_thread_wrapper_struct {
  prob_dist_t** prob_dist;
  pnode_t** ptree_current_ply;
  pnode_t** result_ply; 
  long long cardinality;
  long long goal;
  int test;
} ptree_thread_wrapper;


// Ruby-Accessible API
static VALUE ptree_alloc(VALUE klass);
static VALUE ptree_init(VALUE self, VALUE prob_dists, VALUE goal_hash);
static VALUE ptree_cardinality(VALUE self); // Useful for building our ply arrays...
static VALUE ptree_success_prob(VALUE self);
static VALUE ptree_run_once(VALUE self);
static VALUE ptree_next_ply(VALUE self);
static VALUE ptree_next_prob_dist(VALUE self);
static VALUE pnode2rbstr(pnode_t* node);
static VALUE ptree_run_once(VALUE self);

// Hidden C internal stuff
static void ptree_free(ptree_t* self);
static void ptree_init_goal(VALUE self, VALUE goal_hash);
static void ptree_init_cardinality(VALUE self, VALUE goal_hash);
static void ptree_init_plies(VALUE self);
static void ptree_swap_plies(VALUE self);
static pnode_t* pnode_create(double probspace, long long successes, int attempts);
static pnode_t** pnode_ply_create(long long cardinality);
static void pnode_init(pnode_t* self);
static void pnode_set(pnode_t* self, double probspace, long long successes, int attempts);
static int ptree_ply_location_for_successes(VALUE self, long long successes);
static void ptree_gen_children(VALUE self, pnode_t* parent, VALUE prob_dist, pnode_t** destination_ply);
static void write_to_destination_ply(VALUE self, pnode_t** destination_ply, long long successes, double probspace, int attempts);

/*
 * The multithreaded functions cannot/should not access the ruby API.
 * The single threaded version accesses the api way more than it should honestly
 * To combat this we will be data loading and passing everything we need into the different threads.
 */
//Defines for threaded functions
//Function pointer, so we can pass this as reference into the threads.
static void *ptree_threaded_next_ply(ptree_thread_wrapper** wrapper);
static void ptree_init_prob_dists(VALUE self);
#endif
