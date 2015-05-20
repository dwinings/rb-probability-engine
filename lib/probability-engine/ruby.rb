dir = File.expand_path(File.dirname(__FILE__))
Dir.glob("#{dir}/ruby/**/*.rb") { |f| require f }
