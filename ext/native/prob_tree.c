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
  pnode_t** ply_a;
  pnode_t** ply_b;
  pnode_t** current_ply;
  long goal;
  long long cardinality;
  int current_prob_dist;
  int depth;
} ptree_t;


// Ruby-Accessible API

static VALUE ptree_alloc(VALUE klass);
static VALUE ptree_init(VALUE self, VALUE prob_dists, VALUE goal_hash);
static VALUE ptree_cardinality(VALUE self); // Useful for building our ply arrays...
static VALUE ptree_success_prob(VALUE self);
// static VALUE ptree_num_nodes(VALUE self);
// static VALUE ptree_discarded_nodes(VALUE self);
// static VALUE ptree_run_once(VALUE self);
// static VALUE ptree_run(VALUE self, VALUE iters);
// static VALUE ptree_next_ply(VALUE self);

// Hidden C internal stuff

static void ptree_free(ptree_t* self);
static void ptree_init_goal(VALUE self, VALUE goal_hash);
static void ptree_init_cardinality(VALUE self, VALUE goal_hash);
static void ptree_init_plies(VALUE self);
static void ptree_swap_plies(VALUE self);
static pnode_t* pnode_create(double probspace, long long successes);
static pnode_t** pnode_ply_create(long long cardinality);
static void pnode_init(pnode_t* self);
static void pnode_set(pnode_t* self, double probspace, long long successes);
static int ptree_ply_location_for_successes(VALUE self, long long successes);
// static void ptree_create_or_reuse_node(pnode_t* pnode);
// static void ptree_gen_children(VALUE self, pnode_t* parent);
// static void ptree_victorious_nodes(VALUE self);
// static void pnode_met_goal(pnode_t node, long goal);
// static void ptree_incr_dist(VALUE self);

////////////////////////////////////////////////////////////////////////////////

static VALUE ptree_alloc(VALUE klass) {
  ptree_t* ptree = (ptree_t*)malloc(sizeof(ptree_t));
  // TODO: Get cardinality and init plies.
  // TODO: Init the goal
  ptree->goal = 0L;
  ptree->current_prob_dist = 0;
  ptree->depth = 0;
  return Data_Wrap_Struct(klass, 0, ptree_free, ptree);
}
static VALUE to_a(VALUE thing) {
  return rb_funcall(thing, rb_intern("to_a"), 0);
}
static VALUE ptree_init(VALUE self, VALUE prob_dists, VALUE goal_hash) {
  // prob_dists.map(&:to_a)
  prob_dists = rb_block_call(prob_dists, rb_intern("map"), 0, NULL, to_a, Qtrue);
  rb_iv_set(self, "@prob_dists", prob_dists);
  ptree_init_goal(self, goal_hash);
  ptree_init_cardinality(self, goal_hash);
  ptree_init_plies(self);

  pnode_t* foo = pnode_create(1.0, 0x0303); 
  printf("Node Location in Ply: %d\n", ptree_ply_location_for_successes(self, foo->successes));
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

static int ptree_ply_location_for_successes(VALUE self, long long successes) {
  VALUE goal_hash, type_lookup, type_lookup_keys;
  VALUE* c_type_lookup_keys;
  int node_location = 0;
  int goal_multiplier = 1;

  type_lookup = rb_iv_get(self, "@type_lookup");
  goal_hash = rb_iv_get(self, "@type_len_lookup");

  type_lookup_keys = rb_funcall(goal_hash, rb_intern("keys"), 0);
  c_type_lookup_keys = RARRAY_PTR(type_lookup_keys);
  for(int i=0; i < RARRAY_LEN(type_lookup_keys); i++) {
    int index = FIX2INT(rb_hash_aref(type_lookup, c_type_lookup_keys[i]))*8;
    int current_success = ((successes) >> index) & 0xFF; 
    node_location+=current_success*goal_multiplier;

    goal_multiplier *= FIX2INT(rb_hash_aref(goal_hash, c_type_lookup_keys[i]))+1;
  }
  return node_location;
}

static VALUE pnode2rbstr(pnode_t* pnode) {
  char* out = malloc(sizeof(char)*255);
  sprintf(out, "<PNode: @probspace=%f, @successes=%lld>", pnode->probspace, pnode->successes);
  return rb_str_new2(out);
}

static VALUE ptree_current_ply(VALUE self) {
  return rb_cObject;
}


static VALUE ptree_cardinality(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  return LONG2FIX(ptree->cardinality);
}

static void ptree_init_goal(VALUE self, VALUE goal_hash) {
  ptree_t* ptree;
  VALUE type_lookup, goal_types_ary, goal_type, goal_num;
  int i;

  Data_Get_Struct(self, ptree_t, ptree);
  type_lookup = rb_hash_new();
  goal_types_ary = rb_funcall(goal_hash, rb_intern("keys"), 0);
  for (i=0; i < RARRAY_LEN(goal_types_ary); i++) {
    goal_type = rb_ary_entry(goal_types_ary, i);
    goal_num = FIX2INT(rb_hash_aref(goal_hash, goal_type));
    rb_hash_aset(type_lookup, goal_type, INT2FIX(i));
    ptree->goal += goal_num << (i*8); // Bitpacking adventure!
  }
  // You *must* use the correct ruby sigils for these things.
  rb_iv_set(self, "@type_lookup", type_lookup);
  rb_iv_set(self, "@type_len_lookup", goal_hash);
  return;
}

static void ptree_init_plies(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  //Ply create uses calloc so mem is 0
  ptree->ply_a = pnode_ply_create(ptree->cardinality);
  ptree->ply_b = pnode_ply_create(ptree->cardinality);
  ptree->current_ply = ptree->ply_a;
}

static void ptree_swap_plies(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  if (ptree->current_ply == ptree->ply_a) {
    ptree->current_ply = ptree->ply_b;
  } else {
    ptree->current_ply = ptree->ply_a;
  }
  return;
}

static void ptree_ply_loc_from_success(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
}

static VALUE ptree_goal(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  return INT2FIX(ptree->goal);
}

static pnode_t** pnode_ply_create(long long cardinality) {
  pnode_t** ply;
  if(cardinality > MAX_CARDINALITY) {
    printf("Tree too large!");
    return 0x0;
  } else {
    ply = (pnode_t**)calloc(cardinality, sizeof(pnode_t));
  }
  return ply;
}

static pnode_t* pnode_create(double probspace, long long successes) {
  pnode_t* pnode = (pnode_t*)malloc(sizeof(pnode_t));
  pnode->probspace = probspace;
  pnode->successes = successes;
  return pnode;
}

static void pnode_init(pnode_t* self) {
  self->probspace = 0.0;
  self->successes = 0L;
  return;
}

static void pnode_set(pnode_t* self, double probspace, long long successes) {
  self->probspace = probspace;
  self->successes = successes;
  return;
}

static void ptree_free(ptree_t* ptree) {
  free(ptree->ply_a);
  free(ptree->ply_b);
  free(ptree);
  return;
}

static VALUE ptree_success_prob(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  int goal_location = ptree_ply_location_for_successes(self, ptree->goal);
  if ((ptree->current_ply)[goal_location] == 0) {
    return rb_float_new(0.0f);
  } else {
    return rb_float_new(((ptree->current_ply)[goal_location])->probspace);
  }
}

void Init_native() {
  VALUE cPTree = rb_define_class("ProbTree", rb_cObject);
  //                         Class  rb-name   C-func #-args
  rb_define_alloc_func(cPTree, ptree_alloc);
  rb_define_method(cPTree, "initialize", ptree_init, 2);
  rb_define_method(cPTree, "goal", ptree_goal, 0);
  rb_define_method(cPTree, "cardinality", ptree_cardinality, 0);
  rb_define_method(cPTree, "current_ply", ptree_current_ply, 0);
  rb_define_method(cPTree, "success_prob", ptree_success_prob, 0);
  //                                    r  w
  rb_define_attr(cPTree, "prob_dists", 1, 0);
  rb_define_attr(cPTree, "type_lookup", 1, 0);
  rb_define_attr(cPTree, "type_len_lookup", 1, 0);
}
