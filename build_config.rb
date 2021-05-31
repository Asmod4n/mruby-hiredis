MRuby::Build.new do |conf|
  toolchain :clang
  enable_debug
  conf.enable_sanitizer "address,undefined"
  conf.enable_debug
  conf.enable_bintest
  conf.enable_test
  conf.gembox 'full-core'
  conf.gem File.expand_path(File.dirname(__FILE__))
end
