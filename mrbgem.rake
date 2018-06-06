require_relative 'mrblib/version'

MRuby::Gem::Specification.new('mruby-hiredis') do |spec|
  spec.license = 'Apache-2'
  spec.author  = 'Hendrik'
  spec.summary = 'hiredis bindings for mruby'
  spec.version = Hiredis::VERSION
  spec.add_dependency 'mruby-errno'
  spec.add_dependency 'mruby-redis-ae'

  if build.toolchains.include?('android')
    spec.cc.defines << 'HAVE_PTHREADS'
  else
    spec.linker.libraries << 'pthread'
  end

  if spec.cc.search_header_path('hiredis/hiredis.h') && spec.cc.search_header_path('hiredis/async.h')
    spec.linker.libraries << 'hiredis'
  else
    hiredis_src = "#{spec.dir}/deps"
    spec.cc.include_paths << "#{hiredis_src}"
    spec.objs += %W(
      #{hiredis_src}/hiredis/async.c
      #{hiredis_src}/hiredis/hiredis.c
      #{hiredis_src}/hiredis/net.c
      #{hiredis_src}/hiredis/read.c
      #{hiredis_src}/hiredis/sds.c
    ).map { |f| f.relative_path_from(dir).pathmap("#{build_dir}/%X#{spec.exts.object}" ) }
  end
end
