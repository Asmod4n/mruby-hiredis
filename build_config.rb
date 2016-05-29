MRuby::Build.new do |conf|
  conf.cc.flags << '-fsanitize=address'
  conf.linker.flags << '-fsanitize=address'
  toolchain :gcc
  enable_debug
  conf.enable_test
  conf.gembox 'full-core'
  conf.gem File.expand_path(File.dirname(__FILE__))
end
