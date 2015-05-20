require 'rake/extensiontask'
require 'rspec/core/rake_task'

Rake::ExtensionTask.new do |ext|
  ext.name = 'native'
  ext.ext_dir = 'ext/probability-engine/native'
  ext.lib_dir = 'lib/probability-engine'
  ext.tmp_dir = 'tmp'
end
RSpec::Core::RakeTask.new('spec')

task :compile => :clean
task :spec => :compile

