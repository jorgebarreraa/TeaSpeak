#!/usr/bin/env bash

npx typescript-json-validator ./modules/core/ui-loader/CacheFile.ts || exit 1
npx typescript-json-validator modules/core/ui-loader/ShippedFileInfo.ts  || exit 1
npx typescript-json-validator modules/core/app-updater/UpdateConfigFile.ts || exit 1
npx typescript-json-validator modules/core/app-updater/AppInfoFile.ts || exit 1