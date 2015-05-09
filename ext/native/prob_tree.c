#include "prob_tree.h"

static VALUE ptree_alloc(VALUE klass) {
  ptree_t* ptree = (ptree_t*)malloc(sizeof(ptree_t));
  // TODO: Get cardinality and init plies.
  // TODO: Init the goal
  ptree->goal = 0L;
  ptree->current_prob_dist = 0;
  ptree->depth = 0;
  return Data_Wrap_Struct(klass, 0, ptree_free, ptree);
}

static VALUE prob_dist_keys_block(VALUE yielded_dist, VALUE context, int argc, VALUE argv[]) {
  return rb_funcall(yielded_dist, rb_intern("keys"), 0);
}

static VALUE ptree_init(VALUE self, VALUE prob_dists, VALUE goal_hash) {
  rb_iv_set(self, "@prob_dists", prob_dists);
  rb_iv_set(self, "@depth", INT2FIX(0));
  ptree_init_prob_dists(self, prob_dists, goal_hash);
  ptree_init_cardinality(self, goal_hash);
  ptree_init_plies(self);

  return self;
}

VALUE hash_key_renamer_block(VALUE key, VALUE context_ary, int argc, VALUE argv[]) {
  VALUE* context_c_ary = RARRAY_PTR(context_ary);
  VALUE goal_hash = context_c_ary[0];
  VALUE mapper_hash = context_c_ary[1];
  // goal[mapper[key]] = goal.delete(key) if mapper[key]
  if (rb_hash_aref(mapper_hash, key) != Qnil) {
    rb_hash_aset(goal_hash, rb_hash_aref(mapper_hash, key), rb_funcall(goal_hash, rb_intern("delete"), 1, key));
  }
  return Qnil;
}

void rename_keys(VALUE goal_hash, VALUE mapper_hash) {
  // Remap all the keys in the goal hash to use the numeric ones.
  rb_block_call(
                rb_funcall(goal_hash, rb_intern("keys"), 0),
                rb_intern("each"),
                0,
                NULL,
                RUBY_METHOD_FUNC(hash_key_renamer_block),
                rb_ary_new3(2, goal_hash, mapper_hash));
}

static void ptree_init_prob_dists(VALUE self, VALUE prob_dists, VALUE goal_hash) {
  //                          target       method        argc argv   &block                             block_context
  VALUE keys = rb_block_call(prob_dists, rb_intern("map"), 0, NULL, RUBY_METHOD_FUNC(prob_dist_keys_block), self);
  keys = rb_funcall(keys, rb_intern("flatten"), 0);
  keys = rb_funcall(keys, rb_intern("uniq"), 0);
  PUTS_DEBUG(rb_funcall(keys, rb_intern("inspect"), 0));
  ptree_t* ptree = NULL;

  Data_Get_Struct(self, ptree_t, ptree);

  long keys_len;
  VALUE key_to_index_hash = rb_hash_new();
  VALUE* keys_rb_ary;

  keys_len = RARRAY_LEN(keys);
  ptree->num_items = keys_len;
  ptree->item_nums = (int*)calloc(ptree->num_items, sizeof(int));
  keys_rb_ary = RARRAY_PTR(keys);

  for (int i = 0; i < keys_len; i++) {
    rb_hash_aset(key_to_index_hash, keys_rb_ary[i], INT2FIX(i));
  }

  rename_keys(goal_hash, key_to_index_hash);

  for (int i = 0, goal_idx = 0; i < ptree->num_items; i++) {
    VALUE rb_num_items_desired = rb_hash_aref(goal_hash, INT2FIX(i));
    int num_items_desired;
    if (rb_num_items_desired == Qnil) {
      num_items_desired = 0;
    } else {
      num_items_desired = FIX2INT(rb_num_items_desired);
    }

    DEBUG("Desiring %d of item %d\n", num_items_desired, i);
    ptree->item_nums[i] = num_items_desired;
    if (num_items_desired != 0) {
      ptree->goal += num_items_desired << (goal_idx*8);
      goal_idx++;
    }
  }

  DEBUG("PTree Goal initialized to %lld\n", ptree->goal);

  long prob_dist_len = RARRAY_LEN(prob_dists);
  ptree->num_prob_dists = prob_dist_len;
  DEBUG("ptree->num_prob_dists: %ld\n", ptree->num_prob_dists);
  ptree->prob_dists = (outcome_t*)calloc(keys_len*prob_dist_len, sizeof(outcome_t));
  VALUE* prob_dists_ary = RARRAY_PTR(prob_dists);
  PUTS_DEBUG(key_to_index_hash);

  for (long i = 0; i < prob_dist_len; i++) {
    VALUE prob_dist = prob_dists_ary[i];
    rename_keys(prob_dist, key_to_index_hash);
    PUTS_DEBUG(prob_dist);
    for (long j = 0; j < ptree->num_items; j++) {
      outcome_t* outcome = &(ptree->prob_dists)[(i*ptree->num_items)+j];
      VALUE rb_outcome_hash = rb_hash_aref(prob_dist, INT2FIX(j));
      if (rb_outcome_hash != Qnil) {
        VALUE tmp;
        outcome->item        = j;
        tmp                  = rb_hash_aref(rb_outcome_hash, SYM("reward"));
        if (tmp == Qnil) {
          outcome->quantity = 0;
        } else {
          outcome->quantity    = FIX2LONG(tmp);
        }
        tmp                  = rb_hash_aref(rb_outcome_hash, SYM("prob"));
        if (tmp == Qnil) {
          outcome->probability = 0.0f;
        } else {
          outcome->probability = NUM2DBL(tmp);
        }
        outcome->initialized = 1;
      } else {
        outcome->initialized = 0;
      }
    }
  }
  DEBUG("ptree->num_items: %lld\n", ptree->num_items);
  print_all_outcomes(ptree);
  DEBUG("INITALIZATION COMPLETE\n");
}

void print_all_outcomes(ptree_t* ptree) {
  for (long i = 0; i < ptree->num_prob_dists; i++) {
    DEBUG("New prob dist.\n");
    for (long j = 0; j < ptree->num_items; j++) {
      print_outcome(get_outcome(ptree, i, j));
    }
  }
}

static void print_outcome(outcome_t* outcome) {
  if (outcome != NULL) {
    if (outcome->initialized != 0) {
      DEBUG("<Outcome: @item=%ld, @qty=%ld, @prob=%0.10f%%\n", outcome->item, outcome->quantity, outcome->probability * 100);
    } else {
      DEBUG("<Outcome: NONE>\n");
    }
  }
}

static outcome_t* get_outcome(ptree_t* ptree, long prob_dist_num, long outcome_num) {
  // DEBUG("From pdist #%ld, getting outcome #%ld (#%ld)\n", prob_dist_num, outcome_num, ((ptree->num_items)*prob_dist_num) + outcome_num);
  return &((ptree->prob_dists)[((ptree->num_items)*prob_dist_num) + outcome_num]);
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
  DEBUG("ptree->cardinality: %lld\n", ptree->cardinality);
  return;
}

static long long ptree_ply_location_for_successes(ptree_t* ptree, long long successes) {
  long long node_location = 0, goal_multiplier = 1;

  for(int i = 0, goal_idx = 0; i < ptree->num_items; i++) {
    if (ptree->item_nums[i] > 0) {
      long long current_success = (successes >> (goal_idx*8)) & 0xFF;
      node_location += current_success * goal_multiplier;
      goal_multiplier *= ptree->item_nums[i] + 1;
      goal_idx++;
    }
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

static void ptree_init_plies(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  //Ply create uses calloc so mem is 0
  ptree->current_ply = pnode_ply_create(ptree->cardinality);
  ptree->next_ply = pnode_ply_create(ptree->cardinality);

  write_to_destination_ply(ptree, ptree->current_ply, 0, 1.0f, 0);
}

static void ptree_swap_plies(ptree_t* ptree) {
  ptree -> current_ply =  (pnode_t**) ((long long) ptree -> current_ply ^ (long long) ptree->next_ply);
  ptree -> next_ply =  (pnode_t**) ((long long) ptree -> next_ply ^ (long long) ptree->current_ply);
  ptree -> current_ply =  (pnode_t**) ((long long) ptree -> current_ply ^ (long long) ptree->next_ply);
  return;
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

  long long goal_location = ptree_ply_location_for_successes(ptree, ptree->goal);
  if ((ptree->current_ply)[goal_location] == 0) {
    return rb_float_new(0.0f);
  } else {
    return rb_float_new(((ptree->current_ply)[goal_location])->probspace);
  }
}

static void ptree_gen_children(ptree_t* ptree, pnode_t* node, long prob_dist_num, pnode_t** destination_ply) {
  outcome_t* outcome;
  double reward_prob, new_probspace;
  long long new_successes, goal_of_type, successes_of_type, reward_of_type;
  int i, goal_idx;

  DEBUG("----- <%5lld, %2.5f%%> -----\n", node->successes, node->probspace);

  for (i = 0, goal_idx = 0; i < ptree->num_items; i++) {
    outcome = get_outcome(ptree, prob_dist_num, i);
    if (outcome->initialized) {
      reward_of_type = outcome->quantity;
      reward_prob = outcome->probability;
      new_successes = node->successes;
      if(ptree->item_nums[i] > 0) {
        if (
            (goal_of_type = (ptree->goal >> (goal_idx*8)) & 0xFF ) >
            (successes_of_type = ((node->successes >> (goal_idx*8)) & 0xFF))) {
          if (successes_of_type + reward_of_type > goal_of_type) {
            reward_of_type = goal_of_type - successes_of_type;
          }
          new_successes = node->successes + (reward_of_type << (goal_idx*8));
          new_successes = ptree->goal < new_successes ? ptree->goal : new_successes;
        }
        goal_idx++;
      }
      new_probspace = node->probspace * reward_prob;
      write_to_destination_ply(ptree, destination_ply, new_successes, new_probspace, node->attempts+1);
    }
  }
}

static void write_to_destination_ply(ptree_t* ptree, pnode_t** destination_ply, long long successes, double probspace, int attempts) {
  long long destination_index = ptree_ply_location_for_successes(ptree, successes);
  pnode_t* new_pnode;
  pnode_t* destination_node = destination_ply[destination_index];

  if(destination_node == 0) {
    DEBUG("NEW       %5lld (%2.10f%%)\n", successes, probspace*100);
    new_pnode = pnode_create(probspace, successes, attempts);
    destination_ply[destination_index] = new_pnode;
  } else if(destination_node->attempts < attempts) {
    DEBUG("OVERWRITE %5lld (%2.10f%%)\n", successes, probspace*100);
    pnode_set(destination_ply[destination_index], probspace, successes, attempts);
  } else { // We are merging the probabilities of these two nodes;
    double new_prob = destination_node->probspace + probspace;
    DEBUG("MERGE     %5lld (%2.10f%% -> %2.10f%%)\n", successes, destination_node->probspace * 100, new_prob * 100);
    pnode_set(destination_ply[destination_index], new_prob, successes, attempts);
  }
}

static VALUE ptree_next_ply(VALUE self) {
  long current_prob_dist;
  pnode_t* current_node;
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  current_prob_dist = ptree_next_prob_dist(ptree);

  DEBUG("===============================================\n");
  for(int i=0; i < ptree->cardinality; i++) {
    current_node = ptree->current_ply[i];
    if((current_node !=  0) && (current_node->attempts) >= ptree->depth) {
      ptree_gen_children(ptree, current_node, current_prob_dist, ptree->next_ply);
    }
  }
  rb_iv_set(self, "@depth", FIX2INT(rb_iv_get(self, "@depth")) + 1);
  //Do dragons.
  ptree_swap_plies(ptree);
  return Qnil;
}

static long ptree_next_prob_dist(ptree_t* ptree) {
  if (ptree->current_prob_dist == ptree->num_prob_dists) {
    ptree->current_prob_dist = 0;
  }

  return (ptree->current_prob_dist)++;
}

static VALUE ptree_run_once(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  DEBUG("!!!!!!!!!!!!!!!!! RUNNING ONCE\n");
  for(int i = 0; i < ptree->num_prob_dists; i++) {
    ptree_next_ply(self);
  }
  return Qnil;
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
  rb_define_method(cPTree, "run_once", ptree_run_once, 0);
  //                                    r  w
  rb_define_attr(cPTree, "prob_dists", 1, 0);
  rb_define_attr(cPTree, "type_lookup", 1, 0);
  rb_define_attr(cPTree, "type_len_lookup", 1, 0);
}
