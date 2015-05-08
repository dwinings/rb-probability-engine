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
                 ], {a:10, b:10, c:5})


t0 = Time.now
i = 0
while(p.success_prob < 0.9) do
  i += 1
  puts 'running....'
  p.run_once
  puts p.success_prob
end

t_delta = Time.now - t0

puts "Success Chance: #{p.success_prob}"
puts "Iterations: #{i}"
puts "Time Delta: #{t_delta}s"

