#!/usr/bin/env python
import json
import base64
import sys

if (len(sys.argv) < 2):
	print "Usage: %s settings.myb [mask.png ...]" % sys.argv[0]
	sys.exit(1)

mybJSON = None
with open(sys.argv[1], "rb") as jsonFile:
	mybJSON = json.load(jsonFile)

masks = []
for dataPath in sys.argv[2:]:
	with open(dataPath, "rb") as pngFile:
		data = pngFile.read()
		data = base64.b64encode(data)
		masks.append(data)

if masks:
	imageSettings = {
		"masks": masks
	}

	mybJSON["image_settings"] = imageSettings

output = json.dumps(mybJSON, indent=2, sort_keys=True)
# Fix trailing whitespace in python's json output
output = "\n".join([line.rstrip() for line in output.split("\n")])

print output