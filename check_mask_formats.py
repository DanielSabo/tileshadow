#!/usr/bin/env python
import json
import base64
import sys
import os
import subprocess

if (len(sys.argv) != 2):
    print "Usage: %s search_path" % sys.argv[0]
    sys.exit(1)

for root, dirs, files in os.walk(sys.argv[1]):
    for filename in files:
        filepath = os.path.join(root, filename)
        if (filepath.endswith(".mbi") or filepath.endswith(".myb")):
            with open(filepath, "rb") as jsonFile:
                mybJSON = json.load(jsonFile)
                encodedMasks = mybJSON.get("image_settings", {}).get("masks", [])

            if (encodedMasks):
                print filepath

            for idx, encodedMask in enumerate(encodedMasks):
                data = base64.b64decode(encodedMask)
                fileCmd = subprocess.Popen(["file", "-"], stdin=subprocess.PIPE)
                fileCmd.stdin.write(data)
                fileCmd.communicate()