call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"  
for /r "D:\Prog\imwrap-v6\third_party\sekaiju" %%%%f in (*.exe *.dll) do ( echo %%%%f && dumpbin /headers "%%%%f" | findstr "machine" )  
