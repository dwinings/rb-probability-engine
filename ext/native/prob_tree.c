#include "prob_tree.h"

//Allocate memory for the ptree and init values to 0
//Wrap the ptree struct in  a ruby object
static VALUE ptree_alloc(VALUE klass) {
  ptree_t* ptree = (ptree_t*)malloc(sizeof(ptree_t));
  ptree->goal = 0L;
  ptree->current_prob_dist = 0;
  ptree->depth = 0;
  return Data_Wrap_Struct(klass, 0, ptree_free, ptree);
}

//Wrapper function to set up other parts of the ptree
static VALUE ptree_init(VALUE self, VALUE prob_dists, VALUE goal_hash) {
  rb_iv_set(self, "@prob_dists", prob_dists);
  rb_iv_set(self, "@depth", INT2FIX(0));
  ptree_init_goal(self, goal_hash);
  ptree_init_prob_dists(self);
  ptree_init_cardinality(self, goal_hash);
  ptree_init_plies(self);

  return self;
}

//Create the goal
//The goal will be a long long that is bit packed into 8 bit chunks
//Each chunk represents an item and how many of the item we want
static void ptree_init_goal(VALUE self, VALUE goal_hash) {
  ptree_t* ptree;
  VALUE type_lookup, goal_types_ary, goal_type, goal_num;
  int i;

  //Get ptree
  Data_Get_Struct(self, ptree_t, ptree);

  //New hashes
  type_lookup = rb_hash_new();
  goal_types_ary = rb_funcall(goal_hash, rb_intern("keys"), 0);

  //Iterate over the items we specified as goals.
  for (i=0; i < RARRAY_LEN(goal_types_ary); i++) {
    //key: item name | value: index in goal
    goal_type = rb_ary_entry(goal_types_ary, i);
    //Get the num we want
    goal_num = FIX2INT(rb_hash_aref(goal_hash, goal_type));
    rb_hash_aset(type_lookup, goal_type, INT2FIX(i));

    ptree->goal += goal_num << (i*8); // Bitpacking adventure!
  }
  // Setting ruby hashes
  // You *must* use the correct ruby sigils for these things.
  rb_iv_set(self, "@type_lookup", type_lookup);
  rb_iv_set(self, "@type_len_lookup", goal_hash);
  return;
}

//Initialize the ptrees plies
static void ptree_init_plies(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  //Ply create uses calloc so mem is 0
  ptree->current_ply = pnode_ply_create(ptree->cardinality);
  ptree->next_ply = pnode_ply_create(ptree->cardinality);

  //Setup initial ply to be 1
  write_to_destination_ply(self, ptree->current_ply, 0, 1.0f, 0);
}
/*
 *  Since we can't access the ruby hashes that were created earlier we have to
 *  build some other structure for our list of probability distributions that will
 *  also be safe to pass into other threads. The result is a multi dimmensional array of 
 *  references. 
 */
//Initializing the C structure for prob dists
static void ptree_init_prob_dists(VALUE self) {
  VALUE type_lookup_hash, type_keys_ary, prob_dists_ary, prob_dist_hash, prob_dist_keys_ary, index;
  VALUE* prob_dist_keys_ptr;
  ptree_t* ptree; 

  Data_Get_Struct(self, ptree_t, ptree);
  //Getting all of the ruby objects.
  type_lookup_hash = rb_iv_get(self, "@type_lookup");
  type_keys_ary = rb_funcall(type_lookup_hash, rb_intern("keys"), 0);
  prob_dists_ary = rb_iv_get(self, "@prob_dists");

  //Malloc and set to 0
  ptree->prob_dists = calloc(RARRAY_LEN(prob_dists_ary), sizeof(prob_dist_t));

  //For every probability distribution
  for(int i=0; i < RARRAY_LEN(prob_dists_ary); i++) {
    //Get the distribution hash, the keys, and a pointer to the keys
    prob_dist_hash = rb_ary_entry(prob_dists_ary, i);
    prob_dist_keys_ary = rb_funcall(prob_dist_hash, rb_intern("keys"), 0);
    prob_dist_keys_ptr = RARRAY_PTR(prob_dist_keys_ary);

    //Iterate over the keys
    for(int j=0; j < RARRAY_LEN(prob_dist_keys_ary); j++) {
      //Get where that item was placed in goal
      index = rb_hash_aref(type_lookup_hash, prob_dist_keys_ptr[j]);

      //Set mem for the distribution
      (ptree->prob_dists)[i] = calloc(RARRAY_LEN(prob_dist_keys_ary), sizeof(prob_dist_item_node_t)); 
      //Setup the struct
      (ptree->prob_dists)[i] -> num_prob = RARRAY_LEN(prob_dist_keys_ary);
      (((ptree->prob_dists)[i]) -> probs) = (prob_dist_item_node_t*)malloc(RARRAY_LEN(prob_dist_keys_ary)*sizeof(prob_dist_item_node_t));

        //If it was a goal condition
      if(index != Qnil ) {
        //Initialize and build the probability node
        ((ptree->prob_dists[i]) -> probs)[j] = malloc(sizeof(prob_dist_item_node_t));
        ((ptree->prob_dists)[i] -> probs)[j]->index = FIX2INT(index);
        VALUE prob_hash = rb_hash_aref(prob_dist_hash, prob_dist_keys_ptr[j]);
        ((ptree->prob_dists)[i] -> probs)[j]->reward = FIX2INT(rb_hash_aref(prob_hash, SYM("reward")));
        ((ptree->prob_dists)[i] -> probs)[j]->prob = NUM2DBL(rb_hash_aref(prob_hash, SYM("prob")));
      } else {
        //Otherwise it wasn't a goal condition and we could care less
        ((ptree->prob_dists)[i] -> probs)[j] = 0;
      }
    }
  } 
}

//Simple cardinality calculation
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

//Get the ply location based on the successes
//Just a multidimmensional array calculation.
/*
 * It works the following way. Say we have a goal of 1 a and 1 b. Our states can be
 * 1a, 1b, 1a1bx2. The successes is a long long that is bit packed the same way the goal was.
 * The goal would be packed such that the first chunk represents 1a and the second chunk
 * represents 1b. The successes first chunk represents xa and the second chunk represents xb.
 * We use these as keys to a large array of node locations. We do this because in this example, 
 * we can end up with two different nodes both representing 1a and 1b, but they will map
 * to the same place in the array because the have the same successes value. 
 */
static int ptree_ply_location_for_successes(VALUE self, long long successes) {
  VALUE goal_hash, type_lookup, type_lookup_keys;
  VALUE* c_type_lookup_keys;
  int node_location = 0;
  int goal_multiplier = 1;

  type_lookup = rb_iv_get(self, "@type_lookup");
  goal_hash = rb_iv_get(self, "@type_len_lookup");

  if ((type_lookup_keys = rb_iv_get(self, "@type_lookup_keys")) == Qnil) {
    rb_iv_set(self, "@type_lookup_keys", rb_funcall(goal_hash, rb_intern("keys"), 0));
    type_lookup_keys = rb_iv_get(self, "@type_lookup_keys");
  }

  c_type_lookup_keys = RARRAY_PTR(type_lookup_keys);
  for(int i=0; i < RARRAY_LEN(type_lookup_keys); i++) {
    int index = FIX2INT(rb_hash_aref(type_lookup, c_type_lookup_keys[i]))*8;
    int current_success = ((successes) >> index) & 0xFF; 
    node_location+=current_success*goal_multiplier;

    goal_multiplier *= FIX2INT(rb_hash_aref(goal_hash, c_type_lookup_keys[i]))+1;
  }
  return node_location;
}

//Printing nodes.
static VALUE pnode2rbstr(pnode_t* pnode) {
  char* out = malloc(sizeof(char)*255);
  sprintf(out, "<PNode: @probspace=%0.20f, @successes=%lld>", pnode->probspace, pnode->successes);
  return rb_str_new2(out);
}

//Returns the cardinality. Exposed to the Ruby API
static VALUE ptree_cardinality(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  return LONG2FIX(ptree->cardinality);
}

//Swapping current ply with next ply
static void ptree_swap_plies(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  pnode_t** tmp;
  tmp = ptree->current_ply;
  ptree->current_ply = ptree->next_ply;
  ptree->next_ply = tmp;
  
  //Swap via xor.
  /* 
  ptree -> current_ply =  (pnode_t**) ((long long) ptree -> current_ply ^ (long long) ptree->next_ply);
  ptree -> next_ply =  (pnode_t**) ((long long) ptree -> next_ply ^ (long long) ptree->current_ply);
  ptree -> current_ply =  (pnode_t**) ((long long) ptree -> current_ply ^ (long long) ptree->next_ply);
*/
  return;
}

//Returns the goal. Exposed to the Ruby API
static VALUE ptree_goal(VALUE self) {
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  return INT2FIX(ptree->goal);
}

//Creates a new ply.
static pnode_t** pnode_ply_create(long long cardinality) {
  pnode_t** ply;
  //If we have fewer than the max nodes
  if(cardinality > MAX_CARDINALITY) {
    printf("Tree too large!\n");
    return 0x0;
  } else {
    //Set space for all the nodes and set them all to 0.
    ply = (pnode_t**)calloc(cardinality, sizeof(pnode_t));
  }
  return ply;
}

//Creates a new node given the params.
static pnode_t* pnode_create(double probspace, long long successes, int attempts) {
  pnode_t* pnode = (pnode_t*)malloc(sizeof(pnode_t));
  pnode->probspace = probspace;
  pnode->successes = successes;
  pnode->attempts = attempts;
  return pnode;
}

//Inits a node to 0
static void pnode_init(pnode_t* self) {
  self->probspace = 0.0;
  self->successes = 0L;
  return;
}

//Sets a node given the params
static void pnode_set(pnode_t* self, double probspace, long long successes, int attempts) {
  self->probspace = probspace;
  self->successes = successes;
  self->attempts  = attempts; 
  return;
}

//Delete ptree
static void ptree_free(ptree_t* ptree) {
  free(ptree->current_ply);
  free(ptree->next_ply);
  free(ptree);
  return;
}

//Used to calculate the current probability of the gaol state.
//Exposed to the Ruby API
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

//Writes a new node to the correct place in the given destination ply
static void write_to_destination_ply(VALUE self, pnode_t** destination_ply, long long successes, double probspace, int attempts) {

  //Find the correct index in the ply
  int destination_index = ptree_ply_location_for_successes(self, successes);
  pnode_t* new_pnode;
  pnode_t* destination_node = destination_ply[destination_index];

  //If that node happens to be null
  if(destination_node == 0) {
    //Create it and set it
    new_pnode = pnode_create(probspace, successes, attempts);
    destination_ply[destination_index] = new_pnode;
  } else if(destination_node->attempts < attempts) {
    //Otherwise there was alread stuff there and we are newer information. Overwrite
    pnode_set(destination_ply[destination_index], probspace, successes, attempts); 
  } else { // We are merging the probabilities of these two nodes; 
    //Otherwise, we found a node that mapped to the same place as us on the same ply. 
    //We must aggregate probabilities.
    pnode_set(destination_ply[destination_index], (destination_node->probspace + probspace), successes, attempts);
  }
}

//Used in critical part of the calculation.
//Basically generates the entire next layer of the tree.
static VALUE ptree_next_ply(VALUE self) {
  VALUE current_prob_dist;
  pnode_t* current_node;
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);
  //Get the current prob distribution.
  current_prob_dist = ptree_next_prob_dist(self);

  //For all possible nodes
  for(int i=0; i < ptree->cardinality; i++) {
    //Get the current node.
    current_node = ptree->current_ply[i];
    if((current_node !=  0) && (current_node->attempts) >= FIX2INT(rb_iv_get(self, "@depth"))) {
      //Generate the children given the current node and current prob distribution.
      ptree_gen_children(self, current_node, current_prob_dist, ptree->next_ply);
    }
  }
  rb_iv_set(self, "@depth", FIX2INT(rb_iv_get(self, "@depth")) + 1);
  //Do dragons.
  ptree_swap_plies(self); //We have generated the next ply. Set current ply to next ply and now 
                          // next ply is the old "current" ready to be overwritten.
  return Qnil;
}

//Returns the next probability distribution to generate the next ply.
static VALUE ptree_next_prob_dist(VALUE self) {
  VALUE prob_dists;
  ptree_t* ptree;
  Data_Get_Struct(self, ptree_t, ptree);

  prob_dists = rb_iv_get(self, "@prob_dists");
  if(ptree->current_prob_dist == FIX2INT(rb_funcall(prob_dists, rb_intern("count"), 0))) {
    ptree->current_prob_dist = 0;
  }

  (ptree->current_prob_dist)++;
  return rb_ary_entry(prob_dists, (ptree->current_prob_dist)-1);
}

//Generates the children of a node given that node and a probability distribution
static void ptree_gen_children(VALUE self, pnode_t* node, VALUE prob_dist, pnode_t** destination_ply) {
  VALUE prob_dist_keys, type_lookup, type_id, outcome, reward, prob;
  VALUE* c_prob_dist_keys;
  ptree_t* ptree;
  long long new_successes;
  long long goal_of_type;
  long long successes_of_type;
  long long reward_of_type;
  double new_probspace;

  Data_Get_Struct(self, ptree_t, ptree);

  prob_dist_keys = rb_funcall(prob_dist, rb_intern("keys"), 0);
  c_prob_dist_keys = RARRAY_PTR(prob_dist_keys);

  type_lookup = rb_iv_get(self, "@type_lookup");

  //Iterate over all of the probabilities in the distribution and apply them to the node
  for(int i=0; i<RARRAY_LEN(prob_dist_keys); i++) {
    type_id = rb_hash_aref(type_lookup, c_prob_dist_keys[i]);
    outcome = rb_hash_aref(prob_dist, c_prob_dist_keys[i]);
    reward = rb_hash_aref(outcome, SYM("reward"));
    prob = rb_hash_aref(outcome, SYM("prob"));
    new_successes = node->successes;
    
    if(type_id != Qnil && 
        ( goal_of_type = (ptree->goal >> (FIX2INT(type_id)*8)) & 0xFF ) > ( successes_of_type = ((node->successes >> (FIX2INT(type_id)*8)) & 0xFF) )){
      reward_of_type = FIX2INT(reward);

      //If I have a goal for the current type and my goal is bigger than what the node has generated.
      if (successes_of_type + reward_of_type > goal_of_type) {
        //If I'm not adding more items than Im searching for.
        reward_of_type = goal_of_type - successes_of_type; //Otherwise add only what I am searching for.
      }

      //Add my reward to the new_successes
      new_successes = node->successes + (reward_of_type << (FIX2INT(type_id)*8));
      //If my gaol is less than the success of the node then set to gaol.
      new_successes = ptree->goal < new_successes ? ptree->goal : new_successes;
    } 

    //Simple probability compound...
    new_probspace = node->probspace * NUM2DBL(prob);
    //Finally write my new node out to my specified destination.
    write_to_destination_ply(self, destination_ply, new_successes, new_probspace, node->attempts+1);
  }
}

//Generates children inside of a thread
static void ptree_gen_children_threaded(pnode_t* node, ptree_thread_wrapper** wrapper) {
  (*wrapper) -> test = (*(*wrapper) -> prob_dist) -> num_prob;
  int current_index;
  long long goal_of_type; 
  long long successes_of_type;
  long long reward_of_type;
  double probability_of_type;

  double new_probspace;
  long long new_successes;

  for(int i=0; i<(*(*wrapper)->prob_dist)->num_prob; i++) {
    if((*(*wrapper)->prob_dist)->probs[i] != NULL) {
      current_index = (*(*wrapper)->prob_dist)->probs[i]->index;
      printf("Current index: %d\n", current_index); 
      goal_of_type = (((*wrapper)->goal) >> (current_index*8) & 0xFF);
      successes_of_type = (node->successes >> (current_index*8) & 0xFF);
      if(goal_of_type > successes_of_type) {
        reward_of_type = (*(*wrapper)->prob_dist)->probs[i]->reward;
        probability_of_type = (*(*wrapper) ->prob_dist)->probs[i]->prob;

        if(successes_of_type + reward_of_type > goal_of_type) {
          reward_of_type = goal_of_type - successes_of_type;
        }

        new_successes = node->successes + (reward_of_type << (current_index*8));
        new_successes = (*wrapper)->goal < new_successes ? (*wrapper)->goal : new_successes;
      }
    }
    new_probspace = node->probspace * probability_of_type;
  }
}

//Calculating the plies works slightly differently with the threads
static void *ptree_threaded_next_ply(ptree_thread_wrapper** wrapper) {
  pnode_t* node;

  for(int i=0; i<(*wrapper)->cardinality; i++) {
    node = (*wrapper)->ptree_current_ply[i];
    ptree_gen_children_threaded(node, wrapper);
  } 

    pthread_exit(wrapper);
}

//Run one ply generation
static VALUE ptree_run_once(VALUE self) {
  int num;
  ptree_t* ptree;
  pthread_t* thread;
  pthread_attr_t attr;
  ptree_thread_wrapper** wrapper;

  Data_Get_Struct(self, ptree_t, ptree);
  num = RARRAY_LEN(rb_iv_get(self, "@prob_dists"));
  wrapper = (ptree_thread_wrapper**)calloc(num, sizeof(ptree_thread_wrapper));
  thread = (pthread_t*)calloc(num, sizeof(pthread_t));

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  for(int i=0; i<num; i++) {
    wrapper[i] = malloc(sizeof(ptree_thread_wrapper));
    wrapper[i] -> prob_dist = &((ptree->prob_dists)[i]);
    wrapper[i] -> result_ply = pnode_ply_create(ptree->cardinality);
    wrapper[i] -> ptree_current_ply = ptree->current_ply;
    wrapper[i] -> goal = ptree->goal;
    wrapper[i] -> cardinality = ptree->cardinality;
    wrapper[i] -> test = -1;

    //Generate ply threaded
    pthread_create(&thread[i], &attr, ptree_threaded_next_ply, &(wrapper[i]));

    //Generate a single ply
    ptree_next_ply(self);
  }
  
  //Join our threads
  void* status = 0;
  for(int i=0; i<num; i++) {
    pthread_join(thread[i], &status);
  }


  //TODO:
  //Do cross product of the wrapper structs
  //Set result as the new final ply of the tree and pass that into the next hunt.
  for(int i=0; i<num; i++) {
  }
  return Qnil;
}

//Expose functions to ruby
void Init_native() {
  VALUE cPTree = rb_define_class("ProbTree", rb_cObject);
  //                         Class  rb-name   C-func #-args
  rb_define_alloc_func(cPTree, ptree_alloc);
  rb_define_method(cPTree, "initialize", ptree_init, 2);
  rb_define_method(cPTree, "goal", ptree_goal, 0);
  rb_define_method(cPTree, "cardinality", ptree_cardinality, 0);
  //rb_define_method(cPTree, "current_ply", ptree_current_ply, 0);
  rb_define_method(cPTree, "success_prob", ptree_success_prob, 0);
  rb_define_method(cPTree, "next_ply", ptree_next_ply, 0);
  rb_define_method(cPTree, "run_once", ptree_run_once, 0);
  //                                    r  w
  rb_define_attr(cPTree, "prob_dists", 1, 0);
  rb_define_attr(cPTree, "type_lookup", 1, 0);
  rb_define_attr(cPTree, "type_len_lookup", 1, 0);
}
