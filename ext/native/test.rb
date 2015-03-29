p = ProbTree.new([
                  {
                   a: {reward: 1, prob: 0.5},
                   b: {reward: 2, prob: 0.1},
                   c: {reward: 1, prob: 0.01},
                   failure: {prob: 0.39}
                  }
                 ], {a: 3, b: 6, c: 4})

p.prob_dists.each {|e| puts e.inspect}
puts p.type_lookup
puts p.type_len_lookup
puts p.goal
puts p.cardinality
puts p.current_ply
