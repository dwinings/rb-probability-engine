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
// static VALUE ptree_run_once(VALUE self);
// static VALUE ptree_run(VALUE self, VALUE iters);
static VALUE ptree_next_ply(VALUE self);
static VALUE ptree_next_prob_dist(VALUE self);
static VALUE pnode2rbstr(pnode_t* node);

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
  //prob_dists = rb_block_call(prob_dists, rb_intern("map"), 0, NULL, to_a, Qtrue);
  rb_iv_set(self, "@prob_dists", prob_dists);
  rb_iv_set(self, "@depth", INT2FIX(0));
  ptree_init_goal(self, goal_hash);
  ptree_init_cardinality(self, goal_hash);
  ptree_init_plies(self);

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
  sprintf(out, "<PNode: @probspace=%0.20f, @successes=%lld>", pnode->probspace, pnode->successes);
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
  ptree->current_ply = pnode_ply_create(ptree->cardinality);
  ptree->next_ply = pnode_ply_create(ptree->cardinality);

  write_to_destination_ply(self, ptree->current_ply, 0, 1.0f, 0);
}

static void ptree_swap_plies(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  //Here be dragons, yo
  ptree -> current_ply =  (pnode_t**) ((long long) ptree -> current_ply ^ (long long) ptree->next_ply);
  ptree -> next_ply =  (pnode_t**) ((long long) ptree -> next_ply ^ (long long) ptree->current_ply);
  ptree -> current_ply =  (pnode_t**) ((long long) ptree -> current_ply ^ (long long) ptree->next_ply);

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
    printf("Tree too large!\n");
    return 0x0;
  } else {
    ply = (pnode_t**)calloc(cardinality, sizeof(pnode_t));
  }
  return ply;
}

static pnode_t* pnode_create(double probspace, long long successes, int attempts) {
  pnode_t* pnode = (pnode_t*)malloc(sizeof(pnode_t));
  pnode->probspace = probspace;
  pnode->successes = successes;
  pnode->attempts = attempts;
  return pnode;
}

static void pnode_init(pnode_t* self) {
  self->probspace = 0.0;
  self->successes = 0L;
  return;
}

static void pnode_set(pnode_t* self, double probspace, long long successes, int attempts) {
  self->probspace = probspace;
  self->successes = successes;
  self->attempts  = attempts; 
  return;
}

static void ptree_free(ptree_t* ptree) {
  free(ptree->current_ply);
  free(ptree->next_ply);
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

static void ptree_gen_children(VALUE self, pnode_t* node, VALUE prob_dist, pnode_t** destination_ply) {
  VALUE prob_dist_keys, type_lookup, type_id, outcome, reward, prob;
  VALUE* c_prob_dist_keys;
  ptree_t* ptree;
  long long new_successes;
  double new_probspace;

  Data_Get_Struct(self, ptree_t, ptree);

  prob_dist_keys = rb_funcall(prob_dist, rb_intern("keys"), 0);
  c_prob_dist_keys = RARRAY_PTR(prob_dist_keys);

  type_lookup = rb_iv_get(self, "@type_lookup");

  for(int i=0; i<RARRAY_LEN(prob_dist_keys); i++) {
    type_id = rb_hash_aref(type_lookup, c_prob_dist_keys[i]);
    outcome = rb_hash_aref(prob_dist, c_prob_dist_keys[i]);
    reward = rb_hash_aref(outcome, SYM("reward"));
    prob = rb_hash_aref(outcome, SYM("prob"));
    new_successes = node->successes;
    if(type_id != Qnil && 
        ((ptree->goal >> (FIX2INT(type_id)*8)) & 0xFF) > ((node->successes >> FIX2INT(type_id)*8) & 0xFF)) {

      new_successes = node->successes + (FIX2INT(reward) << (FIX2INT(type_id)*8));
      new_successes = ptree->goal < new_successes ? ptree->goal : new_successes;
    } 
    new_probspace = node->probspace * NUM2DBL(prob);
    write_to_destination_ply(self, destination_ply, new_successes, new_probspace, node->attempts+1);
  }
}

static void write_to_destination_ply(VALUE self, pnode_t** destination_ply, long long successes, double probspace, int attempts) {
  int destination_index = ptree_ply_location_for_successes(self, successes);
  pnode_t* new_pnode;
  pnode_t* destination_node = destination_ply[destination_index];

  if(destination_node == 0) {
    new_pnode = pnode_create(probspace, successes, attempts);
    destination_ply[destination_index] = new_pnode;
  } else if(destination_node->attempts < attempts) {
    pnode_set(destination_ply[destination_index], probspace, successes, attempts); 
  } else { // We are merging the probabilities of these two nodes; 
    pnode_set(destination_ply[destination_index], (destination_node->probspace + probspace), successes, attempts);
  }
}

static VALUE ptree_next_ply(VALUE self) {
  VALUE current_prob_dist;
  pnode_t* current_node;
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  current_prob_dist = ptree_next_prob_dist(self);

  for(int i=0; i<ptree_cardinality(self); i++) {
    current_node = ptree->current_ply[i];
    if((current_node !=  0) && (current_node->attempts) >= FIX2INT(rb_iv_get(self, "@depth"))) {
      ptree_gen_children(self, current_node, current_prob_dist, ptree->next_ply);
    }
  }
  rb_iv_set(self, "@depth", FIX2INT(rb_iv_get(self, "@depth")) + 1);
  //Do dragons.
  ptree_swap_plies(self);
  return Qnil;
}

static VALUE ptree_next_prob_dist(VALUE self) {
  VALUE prob_dists;
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  prob_dists = rb_iv_get(self, "@prob_dists");
  if(ptree->current_prob_dist == FIX2INT(rb_funcall(prob_dists, rb_intern("count"), 0))) {
    ptree->current_prob_dist = 0;
  }

  (ptree->current_prob_dist)++;
  return rb_ary_entry(prob_dists, ptree->current_prob_dist-1);
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
  rb_define_method(cPTree, "next_ply", ptree_next_ply, 0);
  //                                    r  w
  rb_define_attr(cPTree, "prob_dists", 1, 0);
  rb_define_attr(cPTree, "type_lookup", 1, 0);
  rb_define_attr(cPTree, "type_len_lookup", 1, 0);
}
