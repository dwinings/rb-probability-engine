p = ProbTree.new([
                  {
                   a: {reward: 1, prob: 0.5},
                   b: {reward: 2, prob: 0.1},
                   c: {reward: 1, prob: 0.01},
                   failure: {prob: 0.39}
                  }
                 ], {a: 3, b: 6, c: 1})

p.prob_dists.each {|e| puts e.inspect}
(1..100).each do
  p.next_ply
end
puts p.success_prob
