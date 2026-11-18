@echo off
rem Copyright The Phorepoc Authors
rem SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
rem build_msvc.bat <vcvars64.bat> <outdir> -- host build + run of the phepebble tests (codec; PPoGATT + session) with MSVC.
call %1 >nul 2>&1
cl /nologo /EHsc /W4 /I "%~dp0shim" "%~dp0pebble_codec_test.cpp" "%~dp0..\phepebble_codec.cpp" /Fe"%~2\pebble_codec_test.exe" /Fo"%~2\\" || exit /b 1
cl /nologo /EHsc /W4 /std:c++17 /I "%~dp0shim" "%~dp0pebble_session_test.cpp" "%~dp0..\phepebble_ppogatt.cpp" "%~dp0..\phepebble_session.cpp" "%~dp0..\phepebble_codec.cpp" /Fe"%~2\pebble_session_test.exe" /Fo"%~2\\" || exit /b 1
"%~2\pebble_codec_test.exe" || exit /b 1
"%~2\pebble_session_test.exe"
