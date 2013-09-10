import os, sys, nuke

mydir = os.path.dirname(__file__)

# Add platform-dependent plugins
platform = []
if nuke.env['MACOS']:
  platform.append('mac')
if nuke.env['LINUX']:
  platform.append('linux')
if nuke.env['WIN32']:
  platform.append('win')
platform.append('-')
platform.append(str(nuke.NUKE_VERSION_MAJOR))

nuke.pluginAppendPath(os.path.join(mydir, "plugins-64", ''.join(platform)))
