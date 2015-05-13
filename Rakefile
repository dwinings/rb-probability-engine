require 'rake/extensiontask'
require 'rspec/core/rake_task'

Rake::ExtensionTask.new('native')
RSpec::Core::RakeTask.new('spec')

task :compile => :clean
task :spec => :compile

