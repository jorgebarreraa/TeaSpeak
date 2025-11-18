cd /d %~dp0\..
call npm run compile-tsc
if errorlevel 1 (
   echo "Failed to compile tsc files"
   exit /b %errorlevel%
)

call npm run compile-sass
if errorlevel 1 (
   echo "Failed to compile sass files"
   exit /b %errorlevel%
)

call npm run build-windows-64
if errorlevel 1 (
   echo "Failed to compile build files"
   exit /b %errorlevel%
)

call npm run package-windows-64
if errorlevel 1 (
   echo "Failed to compile package files"
   exit /b %errorlevel%
)