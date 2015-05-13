Gem::Specification.new do |spec|
  spec.name = 'probability-engine'
  spec.version = '0.1'
  spec.summary = 'Calculate the probability of various things'
  spec.description = 'Originally built for www.desiresensor.com'
  spec.email = 'wisp558@gmail.com'
  spec.homepage = 'http://www.desiresensor.com'
  spec.author = 'David Winings'
  spec.bindir = 'bin'
  # spec.executable = 'exec.rb'
  spec.files = Dir['lib/**/*.rb'] + Dir['bin/*'] + Dir['ext/**/*.c'] + Dir['ext/**/*.h'] + Dir['ext/**/extconf.rb']
  spec.platform = Gem::Platform::RUBY # This is the default
  spec.require_paths = [ 'lib', 'ext' ]
  spec.extensions = Dir['ext/**/extconf.rb']
end

