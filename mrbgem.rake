MRuby::Gem::Specification.new('mruby-hiredis') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'hiredis bindings for mruby'
  spec.add_dependency 'mruby-errno'

  if build.toolchains.include?("androideabi")
    spec.cc.flags << '-DHAVE_PTHREADS'
  else
    spec.linker.libraries << 'pthread'
  end
end
