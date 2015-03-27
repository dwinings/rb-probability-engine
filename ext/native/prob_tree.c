#include <ruby.h>

typedef struct prob_node {
  long successes;
  double probspace;
} pnode_t;

typedef struct prob_tree {
  pnode_t* ply_a;
  pnode_t* ply_b;
  long goal;
  // Hash
  int current_prob_dist;
  int counter;
} ptree_t;


// Ruby-Accessible API

VALUE ptree_new(VALUE self, VALUE prob_dists, VALUE goal_hash);
VALUE ptree_cardinality(VALUE self); // Useful for building our ply arrays...
VALUE ptree_num_nodes(VALUE self);
VALUE ptree_discarded_nodes(VALUE self);
VALUE ptree_run_once(VALUE self);
VALUE ptree_run(VALUE self, VALUE iters);
VALUE ptree_next_ply(VALUE self);

// Hidden C internal stuff

void ptree_free(VALUE self);
void ptree_create_goal(VALUE goal_hash);
void ptree_create_or_reuse_node(pnode_t* pnode);
void ptree_gen_children(VALUE self, pnode_t* parent);
void ptree_victorious_nodes(VALUE self);
void ptree_victorious_prob(VALUE self);
void pnode_met_goal(pnode_t node, long goal);
void ptree_incr_dist(VALUE self);

////////////////////////////////////////////////////////////////////////////////

VALUE ptree_new(VALUE klass, VALUE prob_dists, VALUE goal_hash) {
  ptree_t* ptree = (ptree_t*)malloc(sizeof(ptree_t));
  // TODO: Get cardinality and init plies.
  // TODO: Init the goal
  ptree->goal = 0L;
  ptree->current_prob_dist = 0;
  ptree->counter = 0;
  return Data_Wrap_Struct(klass, 0, free, ptree);
}

void ptree_free(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  // free(ptree->ply_a);
  // free(ptree->ply_b);
  free(ptree);
}

void Init_native() {
  VALUE cPTree = rb_define_class("ProbTree", rb_cObject);
  //                         Class  rb-name   C-func #-args
  rb_define_singleton_method(cPTree, "new", ptree_new, 2);
}
