@echo off

set drive=T:


net use z: /dele > nul
if %ERRORLEVEL% neq 0 (
	if %ERRORLEVEL% neq 2 (
	   echo Failed to unmount old drive [%drive%\]: %errorlevel%.
	   exit /b %errorlevel%
	) else (
		echo Drive [%drive%\] hasn't been mapped. Mapping device!
	)
) else (
	echo Unmounted old device. Mapping new device!
)
rem  /persistent:yes
net use z: \\deploy.teaspeak.de\symbols T4j7CTADvCMev4Kr /user:deploy.teaspeak.de\TeaSpeak-Jenkins-Client
if %errorlevel% neq 0 (
   echo Failed to mount drive [%drive%\]: %errorlevel%
   exit /b %errorlevel%
) else (
	echo New device has been mapped successfully!
)
