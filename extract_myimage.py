#!/usr/bin/env python
import json
import base64
import sys
import os

if (len(sys.argv) != 2):
	print "Usage: %s settings.mbi" % sys.argv[0]
	sys.exit(1)

mybJSON = None
with open(sys.argv[1], "rb") as jsonFile:
	mybJSON = json.load(jsonFile)

prefix = os.path.basename(sys.argv[1]).rsplit(".")[0]
if not prefix:
	sys.exit(1)

encodedMasks = mybJSON.get("image_settings", {}).get("masks", [])

for idx, encodedMask in enumerate(encodedMasks):
	data = base64.b64decode(encodedMask)
	savePath = "{0}_mask{1}.png".format(prefix, idx)
	print savePath
	with open(savePath, "wb") as saveFile:
		saveFile.write(data)