@IF /I NOT "%~1" == "KEEP" IF EXIST bld RMDIR /S /Q bld
cmake -Bbld .
@PUSHD bld
@FOR %%X IN (msbuild.exe) DO @IF NOT "%%~$PATH:X" == "" @CALL :msvc
@POPD
@GOTO:EOF

:msvc
@ECHO. ------------------------------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@ECHO. -------- building Debug ------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
msbuild gravity.sln /p:Configuration=Debug

@ECHO. ------------------------------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@ECHO. -------- building Release ----------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
msbuild gravity.sln /p:Configuration=Release

@ECHO. ------------------------------------------------------------------------------
@ECHO. -- packaging files -----------------------------------------------------------
@ECHO. ------------------------------------------------------------------------------
@(
	MKDIR Debug\bin
	MKDIR Debug\lib

	MOVE bin\Debug\gravity.exe Debug\bin\gravity.exe
	MOVE bin\Debug\gravity.pdb Debug\bin\gravity.pdb
	MOVE src\Debug\gravityapi.dll Debug\bin\gravityapi.dll
	MOVE src\Debug\gravityapi.pdb Debug\bin\gravityapi.pdb
	MOVE lib\Debug\gravityapi.lib Debug\bin\lib\gravityapi.lib
	MOVE lib\Debug\gravityapi.exp Debug\bin\lib\gravityapi.exp
	MOVE lib\Debug\gravityapi_s.lib Debug\lib\gravityapi_s.lib
	MOVE lib\Debug\gravityapi_s.pdb Debug\lib\gravityapi_s.pdb

	RMDIR /S /Q bin\Debug
	RMDIR /S /Q lib\Debug
	RMDIR /S /Q src\Debug
) 1>NUL


@(
	MKDIR Release\bin
	MKDIR Release\lib

	MOVE bin\Release\gravity.exe Release\bin\gravity.exe
	MOVE src\Release\gravityapi.dll Release\bin\gravityapi.dll
	MOVE lib\Release\gravityapi.lib Release\bin\lib\gravityapi.lib
	MOVE lib\Release\gravityapi.exp Release\bin\lib\gravityapi.exp
	MOVE lib\Release\gravityapi_s.lib Release\lib\gravityapi_s.lib

	RMDIR /S /Q bin\Release
	RMDIR /S /Q lib\Release
	RMDIR /S /Q src\Release
) 1>NUL
