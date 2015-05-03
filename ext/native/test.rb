p = ProbTree.new([
                  {
                   a: {reward: 1, prob: 0.5},
                   b: {reward: 2, prob: 0.1},
                   c: {reward: 1, prob: 0.01},
                   failure: {prob: 0.39}
                  },
                  {
                    a: {reward: 4, prob: 0.3},
                    b: {reward: 1, prob: 0.4},
                    failure: {prob: 0.3}
                  }
                 ], {a:1, b:1, c:1})

p.prob_dists.each {|e| puts e.inspect}
i = 0
while(p.success_prob < 0.9) do
  p.run_once
  puts p.success_prob
  i+=1;
end
puts i

