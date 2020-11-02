import subprocess
import os
import json
 
oocdScripts = os.environ["OPENOCD_SCRIPTS"]
oocdConfig = os.environ["OPENOCD_CONFIG"]
buildDir = os.environ["BUILD_DIR"]
 
oocdScripts = oocdScripts.replace("\\", "/")
builoocdConfigdDir = oocdConfig.replace("\\", "/")
buildDir = buildDir.replace("\\", "/")
 
cmd = "openocd.exe -s " + oocdScripts + " " + oocdConfig
 
with open(os.path.join(buildDir, "flasher_args.json")) as f:
    flasher_args = json.load(f)
 
for addr in flasher_args["flash_files"]:
    cmd = cmd + " -c \"program_esp32 " + buildDir + "/" + flasher_args["flash_files"][addr] + " " + addr + " verify exit\""
 
cmd = cmd + " -c \"reset\""
 
# Comment this, if exit from OpenOCD not needed
cmd = cmd + " -c \"exit\""
 
print("Executing:\n" + cmd)
print("")
 
os.system(cmd)