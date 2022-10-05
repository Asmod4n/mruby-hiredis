MRUBY_CONFIG=File.expand_path(ENV["MRUBY_CONFIG"] || "build_config.rb")

file :mruby do
  sh "git clone --depth=1 https://github.com/mruby/mruby.git"
end

desc "compile binary"
task :compile => :mruby do
  sh "cd mruby && MRUBY_CONFIG=#{MRUBY_CONFIG} rake all"
end

desc "test"
task :test => :mruby do
  if ENV['TRAVIS_OS_NAME'] == 'osx'
    sh "brew update && brew install redis && echo 'daemonize yes' >> /usr/local/etc/redis.conf && redis-server /usr/local/etc/redis.conf"
  end
  sh "cd mruby && MRUBY_CONFIG=#{MRUBY_CONFIG} rake all test"
end

desc "cleanup"
task :clean do
  sh "cd mruby && rake deep_clean"
end

task :default => :test
