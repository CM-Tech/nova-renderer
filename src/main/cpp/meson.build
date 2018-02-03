project('nova-renderer', ['c', 'cpp'],default_options : [ 'cpp_std=c++1z'])
vpp_sp=subproject('vpp')
vpp_inc = vpp_sp.get_variable('vpp_inc')
vpp_dep = vpp_sp.get_variable('vpp_dep')

executable('nova-renderer', 'render/nova_renderer.cpp',include_directories: [vpp_inc])