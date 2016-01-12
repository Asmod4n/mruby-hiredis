require_relative 'mrblib/version'

MRuby::Gem::Specification.new('mruby-hiredis') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'hiredis bindings for mruby'
  spec.version = Hiredis::VERSION
  spec.add_dependency 'mruby-errno'
  spec.add_dependency 'mruby-redis-ae'

  if build.toolchains.include?('android')
    spec.cc.flags << '-DHAVE_PTHREADS'
  else
    spec.linker.libraries << 'pthread'
  end
end
