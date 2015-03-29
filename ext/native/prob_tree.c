#include <ruby.h>

#define PUTS(OBJ) (rb_funcall(rb_cObject, rb_intern("puts"), 1, OBJ))

typedef struct prob_node {
  long successes;
  double probspace;
} pnode_t;

typedef struct prob_tree {
  pnode_t* ply_a;
  pnode_t* ply_b;
  long goal;
  long cardinality;
  // Hash
  int current_prob_dist;
  int counter;
} ptree_t;


// Ruby-Accessible API

static VALUE ptree_alloc(VALUE klass);
static VALUE ptree_init(VALUE self, VALUE prob_dists, VALUE goal_hash);
// static VALUE ptree_cardinality(VALUE self); // Useful for building our ply arrays...
// static VALUE ptree_num_nodes(VALUE self);
// static VALUE ptree_discarded_nodes(VALUE self);
// static VALUE ptree_run_once(VALUE self);
// static VALUE ptree_run(VALUE self, VALUE iters);
// static VALUE ptree_next_ply(VALUE self);

// Hidden C internal stuff

static void ptree_free(VALUE self);
static void ptree_init_goal(VALUE self, VALUE goal_hash);
static void ptree_init_cardinality(VALUE self, VALUE goal_hash);
// static void ptree_create_or_reuse_node(pnode_t* pnode);
// static void ptree_gen_children(VALUE self, pnode_t* parent);
// static void ptree_victorious_nodes(VALUE self);
// static void ptree_victorious_prob(VALUE self);
// static void pnode_met_goal(pnode_t node, long goal);
// static void ptree_incr_dist(VALUE self);

////////////////////////////////////////////////////////////////////////////////

static VALUE ptree_alloc(VALUE klass) {
  ptree_t* ptree = (ptree_t*)malloc(sizeof(ptree_t));
  // TODO: Get cardinality and init plies.
  // TODO: Init the goal
  ptree->goal = 0L;
  ptree->current_prob_dist = 0;
  ptree->counter = 0;
  return Data_Wrap_Struct(klass, 0, ptree_free, ptree);
}
static VALUE ptree_init(VALUE self, VALUE prob_dists, VALUE goal_hash) {
  ptree_init_goal(self, goal_hash);
  ptree_init_cardinality(self, goal_hash);
  return self;

}

static void ptree_init_cardinality(VALUE self, VALUE goal_hash) {
  int i, tmp;
  long values_len;
  VALUE value_set;
  VALUE* values_arr;
  ptree_t* ptree;

  Data_Get_Struct(self, ptree_t, ptree);
  value_set = rb_funcall(goal_hash, rb_intern("values"), 0);
  values_len = RARRAY_LEN(value_set);
  values_arr = RARRAY_PTR(value_set);
  tmp = 1;
  for (i=0; i < values_len; i++) {
    tmp *= FIX2INT(values_arr[i]) + 1;
  }
  ptree->cardinality = tmp;
  return;
}

static VALUE ptree_cardinality(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  return LONG2FIX(ptree->cardinality);
}

static void ptree_init_goal(VALUE self, VALUE goal_hash) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  VALUE type_lookup = rb_hash_new();
  int i;
  VALUE goals = rb_funcall(goal_hash, rb_intern("keys"), 0);
  for (i=0; i < RARRAY_LEN(goals); i++) {
    VALUE goal_type = rb_ary_entry(goals, i);
    int goal_num = FIX2INT(rb_hash_aref(goal_hash, goal_type));
    rb_hash_aset(type_lookup, goal_type, INT2FIX(i));
    ptree->goal += goal_num << (i*8);
  }
  // You *must* use the correct ruby sigils for these things.
  rb_iv_set(self, "@type_lookup", type_lookup);
  return;
}

static VALUE ptree_goal(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  return INT2FIX(ptree->goal);
}

static void ptree_free(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  // free(ptree->ply_a);
  // free(ptree->ply_b);
  free(ptree);
}

void Init_native() {
  VALUE cPTree = rb_define_class("ProbTree", rb_cObject);
  //                         Class  rb-name   C-func #-args
  rb_define_alloc_func(cPTree, ptree_alloc);
  rb_define_method(cPTree, "initialize", ptree_init, 2);
  rb_define_method(cPTree, "goal", ptree_goal, 0);
  rb_define_method(cPTree, "cardinality", ptree_cardinality, 0);
  //                                    r  w
  rb_define_attr(cPTree, "type_lookup", 1, 0);
}
