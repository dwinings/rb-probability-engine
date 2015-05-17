#include "prob_tree.h"
#include "prob_tree_helpers.c"

static VALUE ptree_alloc(VALUE klass) {
  ptree_t* ptree;

  ptree = (ptree_t*)malloc(sizeof(ptree_t));
  ptree->goal = 0L;
  ptree->current_prob_dist = 0;
  ptree->depth = 0;
  return Data_Wrap_Struct(klass, 0, ptree_free, ptree);
}

static VALUE ptree_init(VALUE self, VALUE prob_dists, VALUE goal_hash) {
  long item_idx, prob_dist_idx, num_items_desired, goal_idx, prob_dist_len;
  VALUE item_symbol_mapping, item_set, rb_num_items_desired, prob_dist, rb_outcome_hash;
  VALUE* prob_dists_ary;
  ptree_t* ptree;
  outcome_t* outcome;

  Data_Get_Struct(self, ptree_t, ptree);

  // In case ruby programmers forget what they called this thing with.
  rb_iv_set(self, "@prob_dists", prob_dists);
  rb_iv_set(self, "@goals", goal_hash);

  // Initialize item mapping

  //                          target       method        argc argv   &block                             block_context
  item_set = rb_block_call(prob_dists, rb_intern("map"), 0, NULL, RUBY_METHOD_FUNC(prob_dist_keys_block), self);
  item_set = rb_funcall(item_set, rb_intern("flatten"), 0);
  item_set = rb_funcall(item_set, rb_intern("uniq"), 0);
  ptree->num_items = RARRAY_LEN(item_set);

  item_symbol_mapping = rb_hash_new();

  for (item_idx = 0; item_idx < ptree->num_items; item_idx++) {
    rb_hash_aset(item_symbol_mapping, (RARRAY_PTR(item_set))[item_idx], INT2FIX(item_idx));
  }

  // Initialize the goal

  ptree->goals_by_item = (long*)calloc(ptree->num_items, sizeof(long));
  rename_keys(goal_hash, item_symbol_mapping);

  for (item_idx = 0, goal_idx = 0; item_idx < ptree->num_items; item_idx++) {
    rb_num_items_desired = rb_hash_aref(goal_hash, INT2FIX(item_idx));
    if (rb_num_items_desired == Qnil) {
      num_items_desired = 0;
    } else {
      num_items_desired = FIX2INT(rb_num_items_desired);
    }

    DEBUG("Desiring %d of item %d\n", num_items_desired, item_idx);
    ptree->goals_by_item[item_idx] = num_items_desired;
    if (num_items_desired != 0) {
      ptree->goal += num_items_desired << (goal_idx*8);
      goal_idx++;
    }
  }

  // Initialize the cardinality (i.e. the max size of a ply)
  ptree->cardinality = 1;
  for (item_idx = 0; item_idx < ptree->num_items; item_idx++) {
    // If it's 0 it'll just multiply by 1
    ptree->cardinality *= ptree->goals_by_item[item_idx] + 1;
  }

  DEBUG("PTree Goal initialized to %lld\n", ptree->goal);

  // Initialize the Probability Distribution

  prob_dist_len = RARRAY_LEN(prob_dists);
  ptree->num_prob_dists = prob_dist_len;
  DEBUG("ptree->num_prob_dists: %ld\n", ptree->num_prob_dists);
  ptree->prob_dists = (outcome_t*)calloc((ptree->num_items)*prob_dist_len, sizeof(outcome_t));
  prob_dists_ary = RARRAY_PTR(prob_dists);
  PUTS_DEBUG(item_symbol_mapping);

  // for every probability distribution...
  for (prob_dist_idx = 0; prob_dist_idx < prob_dist_len; prob_dist_idx++) {
    prob_dist = prob_dists_ary[prob_dist_idx];
    rename_keys(prob_dist, item_symbol_mapping);
    PUTS_DEBUG(prob_dist);

    // and for every outcome within that distribution...
    for (item_idx = 0; item_idx < ptree->num_items; item_idx++) {
      outcome = &(ptree->prob_dists)[(prob_dist_idx * ptree->num_items) + item_idx];
      rb_outcome_hash = rb_hash_aref(prob_dist, INT2FIX(item_idx));
      initialize_outcome(outcome, item_idx, rb_outcome_hash);
    }
  }
  DEBUG("ptree->num_items: %lld\n", ptree->num_items);
  print_all_outcomes(ptree);

  ptree->current_ply = pnode_ply_create(ptree->cardinality);
  ptree->next_ply = pnode_ply_create(ptree->cardinality);
  write_to_destination_ply(ptree, ptree->current_ply, 0, 1.0f, 0);
  ptree->depth = 0;
  DEBUG("INITALIZATION COMPLETE\n");

  return self;
}

// Initialize an outcome in the ptree struct from a ruby outcome hash's contents
static void initialize_outcome(outcome_t* outcome, long item, VALUE rb_outcome_hash) {
  VALUE tmp;

  if (rb_outcome_hash != Qnil) {
    outcome->item          = item;
    tmp                    = rb_hash_aref(rb_outcome_hash, SYM("reward"));
    if (tmp == Qnil) {
      outcome->quantity    = 0;
    } else {
      outcome->quantity    = FIX2LONG(tmp);
    }
    tmp                    = rb_hash_aref(rb_outcome_hash, SYM("prob"));
    if (tmp == Qnil) {
      outcome->probability = 0.0f;
    } else {
      outcome->probability = NUM2DBL(tmp);
    }
    outcome->initialized   = 1;
  } else {
    outcome->initialized   = 0;
  }
}


static outcome_t* get_outcome(ptree_t* ptree, long prob_dist_num, long outcome_num) {
  return &((ptree->prob_dists)[(ptree->num_items * prob_dist_num) + outcome_num]);
}

static long long ptree_ply_location_for_successes(ptree_t* ptree, long long successes) {
  long long node_location, goal_multiplier, current_success;
  long item_idx, goal_idx;

  node_location = 0;
  goal_multiplier = 1;

  for(item_idx = 0, goal_idx = 0; item_idx < ptree->num_items; item_idx++) {
    if (ptree->goals_by_item[item_idx] > 0) {
      current_success = (successes >> (goal_idx*8)) & 0xFF;
      node_location += current_success * goal_multiplier;
      goal_multiplier *= ptree->goals_by_item[item_idx] + 1;
      goal_idx++;
    }
  }
  return node_location;
}

static void ptree_swap_plies(ptree_t* ptree) {
  // Standard XOR swapping algorithm.
  ptree -> current_ply =  (pnode_t**) ((long long) ptree->current_ply ^ (long long) ptree->next_ply);
  ptree -> next_ply    =  (pnode_t**) ((long long) ptree->next_ply    ^ (long long) ptree->current_ply);
  ptree -> current_ply =  (pnode_t**) ((long long) ptree->current_ply ^ (long long) ptree->next_ply);
  return;
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

static void ptree_free(ptree_t* ptree) {
  int idx;

  free(ptree->prob_dists);
  free(ptree->goals_by_item);

  for (idx = 0; idx < ptree->cardinality; idx++) {
    // Note that pnode_free handles the null check.
    pnode_free((ptree->current_ply)[idx]);
    pnode_free((ptree->next_ply)[idx]);
  }

  free(ptree->current_ply);
  free(ptree->next_ply);
  free(ptree);
  return;
}

static VALUE ptree_success_prob(VALUE self) {
  ptree_t* ptree;
  long long goal_location;

  Data_Get_Struct(self, ptree_t, ptree);

  goal_location = ptree_ply_location_for_successes(ptree, ptree->goal);
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
  int item_idx, goal_idx;

  DEBUG("----- <%5lld, %2.5f%%> -----\n", node->successes, node->probspace);

  // for every item that we know about...
  for (item_idx = 0, goal_idx = 0; item_idx < ptree->num_items; item_idx++) {

    // look at the potential outcome for that item in this probability distribution
    outcome = get_outcome(ptree, prob_dist_num, item_idx);

    // We only generate a node in the ply if the outcome for this item exists
    if (outcome->initialized) {
      reward_of_type = outcome->quantity;
      reward_prob = outcome->probability;
      new_successes = node->successes;

      // if this outcome is relevant to the goal...
      if (ptree->goals_by_item[item_idx] > 0) {
        goal_of_type = (ptree->goal >> (goal_idx*8)) & 0xFF;
        successes_of_type = ((node->successes >> (goal_idx*8)) & 0xFF);

        // ...and we have not reached our target for this item...
        if (goal_of_type > successes_of_type) {
          // (make sure we do not exceed our target for this item)
          if (successes_of_type + reward_of_type > goal_of_type) {
            reward_of_type = goal_of_type - successes_of_type;
          }
          // Make a new node with the update success state!
          new_successes = node->successes + (reward_of_type << (goal_idx*8));
          new_successes = ptree->goal < new_successes ? ptree->goal : new_successes;
        }
      }

      // Even if we didn't update the success state, we should still write the node into the next ply
      new_probspace = node->probspace * reward_prob;
      write_to_destination_ply(ptree, destination_ply, new_successes, new_probspace, node->attempts+1);
    }

    // We need to move to the correct goal index even if we have no outcome in this dist.
    if(ptree->goals_by_item[item_idx] > 0) {
      goal_idx++;
    }
  }
}

static void write_to_destination_ply(ptree_t* ptree, pnode_t** destination_ply, long long successes, double probspace, int attempts) {
  long long destination_index = ptree_ply_location_for_successes(ptree, successes);
  pnode_t* destination_node = destination_ply[destination_index];

  // Either the node is nonexistant (we used calloc() so it's 0) or it is stale or it exists.
  // We create a new node, overwrite the old node, or merge with a duplicate respectively.
  if(destination_node == 0) {
    pnode_t* new_pnode;
    new_pnode = pnode_create(probspace, successes, attempts);
    DEBUG("NEW       %5lld (%2.10f%%)\n", successes, probspace*100);
    destination_ply[destination_index] = new_pnode;
  } else if(destination_node->attempts < attempts) {
    DEBUG("OVERWRITE %5lld (%2.10f%%)\n", successes, probspace*100);
    pnode_set(destination_ply[destination_index], probspace, successes, attempts);
  } else {
    double new_prob;
    // this is the ONLY PLACE we add probabilities.
    new_prob = destination_node->probspace + probspace;
    DEBUG("MERGE     %5lld (%2.10f%% -> %2.10f%%)\n", successes, destination_node->probspace * 100, new_prob * 100);
    pnode_set(destination_ply[destination_index], new_prob, successes, attempts);
  }
}

static VALUE ptree_next_ply(VALUE self) {
  long current_prob_dist;
  long ply_idx;
  pnode_t* current_node;
  ptree_t* ptree;

  Data_Get_Struct(self, ptree_t, ptree);
  current_prob_dist = ptree_next_prob_dist(ptree);

  DEBUG("===============================================\n");
  print_prob_dist(ptree, current_prob_dist);

  // For every slot in the ply...
  for(ply_idx = 0; ply_idx < ptree->cardinality; ply_idx++) {
    current_node = ptree->current_ply[ply_idx];
    if(current_node != 0 && (current_node->attempts) >= ptree->depth) {
      // generate its children in the new ply if that slot holds a valid node.
      ptree_gen_children(ptree, current_node, current_prob_dist, ptree->next_ply);
    }
  }

  (ptree->depth)++;
  ptree_swap_plies(ptree);
  return Qnil;
}

static long ptree_next_prob_dist(ptree_t* ptree) {
  if (ptree->current_prob_dist == ptree->num_prob_dists) {
    ptree->current_prob_dist = 0;
  }
  return (ptree->current_prob_dist)++;
}

// Run all the prob dists given.
static VALUE ptree_run_once(VALUE self) {
  ptree_t* ptree;
  int idx;

  Data_Get_Struct(self, ptree_t, ptree);
  DEBUG("!!!!!!!!!!!!!!!!! RUNNING ONCE\n");
  for(idx = 0; idx < ptree->num_prob_dists; idx++) {
    ptree_next_ply(self);
  }
  return Qnil;
}

void Init_native() {
  VALUE cPTree;

  cPTree = rb_define_class("ProbTree", rb_cObject);
  rb_define_alloc_func(cPTree, ptree_alloc);
  //               Class     rb-name      C-func   #-args
  rb_define_method(cPTree, "initialize", ptree_init, 2);
  rb_define_method(cPTree, "goal", ptree_goal, 0);
  rb_define_method(cPTree, "cardinality", ptree_cardinality, 0);
  rb_define_method(cPTree, "success_prob", ptree_success_prob, 0);
  rb_define_method(cPTree, "next_ply", ptree_next_ply, 0);
  rb_define_method(cPTree, "run_once", ptree_run_once, 0);
  //                                    r  w
  rb_define_attr(cPTree, "prob_dists", 1, 0);
  rb_define_attr(cPTree, "goals", 1, 0);
}
