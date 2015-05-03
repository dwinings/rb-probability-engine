#ifndef PROB_TREE_H
#define PROB_TREE_H

#include <ruby.h>

#define MAX_CARDINALITY 100000
#define PUTS(OBJ) (rb_funcall(rb_cObject, rb_intern("puts"), 1, OBJ))
#define SYM(STR) (ID2SYM(rb_intern(STR)))

typedef struct prob_node {
  long long successes;
  double probspace;
  int attempts;
} pnode_t;

typedef struct prob_tree {
  pnode_t** current_ply;
  pnode_t** next_ply;
  long long goal;
  long long cardinality;
  int current_prob_dist;
  int depth;
} ptree_t;


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
#endif
