#pragma once

#include "Engine.h"
#include "Texture.h"

class FDCUtil
{
public:
	FDCUtil() { }
	void InitEngine();
	void Main();
	void HandleError( const char* Exception );
	void ExitEngine();

private:
	void ConvertTexturePkg( const char* PkgPath );
	void ConvertTexturesInPath( const char* Dir );
	void CommitChanges();

private:
	UEngine* Engine = nullptr;
	TMap<FString, UPackage*> ChangedPackages;
	TArray<UPalette*> UnrefPalettes;
	DWORD TotalPrevSize = 0;
	DWORD TotalNewSize = 0;
};
