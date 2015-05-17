#include "prob_tree.h"

// Here lies various drudge functions that are either:
// a) not directly related to the prob tree's operation
//  or
// b) are accessor glue functions to expose internal C state to rubyland.

static VALUE hash_key_renamer_block(VALUE key, VALUE context_ary, int argc, VALUE argv[]) {
  VALUE* context_c_ary = RARRAY_PTR(context_ary);
  VALUE goal_hash = context_c_ary[0];
  VALUE mapper_hash = context_c_ary[1];
  // goal[mapper[key]] = goal.delete(key) if mapper[key]
  if (rb_hash_aref(mapper_hash, key) != Qnil) {
    rb_hash_aset(goal_hash, rb_hash_aref(mapper_hash, key), rb_funcall(goal_hash, rb_intern("delete"), 1, key));
  }
  return Qnil;
}

static void rename_keys(VALUE goal_hash, VALUE mapper_hash) {
  // Remap all the keys in the goal hash to use the numeric ones.
  rb_block_call(
                rb_funcall(goal_hash, rb_intern("keys"), 0),
                rb_intern("each"),
                0,
                NULL,
                RUBY_METHOD_FUNC(hash_key_renamer_block),
                rb_ary_new3(2, goal_hash, mapper_hash));
}

static VALUE prob_dist_keys_block(VALUE yielded_dist, VALUE context, int argc, VALUE argv[]) {
  return rb_funcall(yielded_dist, rb_intern("keys"), 0);
}

void print_all_outcomes(ptree_t* ptree) {
  for (long i = 0; i < ptree->num_prob_dists; i++) {
    DEBUG("New prob dist.\n");
    for (long j = 0; j < ptree->num_items; j++) {
      print_outcome(get_outcome(ptree, i, j));
    }
  }
}

static void print_prob_dist(ptree_t* ptree, long current_prob_dist) {
  DEBUG("PROB_DIST: \n");
  for (int i = 0; i < ptree->num_items; i++) {
    print_outcome(&((ptree->prob_dists)[(current_prob_dist * ptree->num_items) + i]));
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

static void pnode_free(pnode_t* self) {
  if (self != 0) {
    free(self);
  }
}

// RUBY ACCESSORS (need more of them)

static VALUE ptree_cardinality(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  return LONG2FIX(ptree->cardinality);
}

static VALUE ptree_goal(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  return INT2FIX(ptree->goal);
}
