import nuke

# Inject our own node bar
toolbar = nuke.menu("Nodes")
sy = toolbar.addMenu( "SyLens")
nodes = ('SyLens', 'SyCamera', 'SyUV', 'SyShader')
for nodename in nodes:
  sy.addCommand(nodename, 'nuke.createNode("%s")' % nodename)