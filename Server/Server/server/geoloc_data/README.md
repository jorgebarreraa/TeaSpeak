# GeoLocation CSV Files

This directory contains the CSV files required for IP geolocation functionality in TeaSpeak Server.

## Required Files

Place the following 3 CSV files in this directory:

1. **IP2Location.CSV** - IP to location mapping database
2. **ipcat.csv** - IP categorization database
3. **IpToCountry.csv** - IP to country mapping database

## Automatic Installation

During the TeaSpeak build process (`build_teaspeak.sh`), these CSV files will be automatically copied from this directory to:

```
Server/Root/TeaSpeak/Server/server/environment/geoloc/
```

## Note

These files are **optional**. If they are not present, TeaSpeak will build successfully but geolocation features will not be available.

The build script will show:
- ✓ Success message if CSV files are found and copied
- ⚠ Warning message if this directory or CSV files are not found (non-fatal)
