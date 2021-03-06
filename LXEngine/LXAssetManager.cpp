//------------------------------------------------------------------------------------------------------
//
// This is a part of Seetron Engine
//
// Copyright (c) 2018 Nicolas Arques. All rights reserved.
//
//------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "LXAssetManager.h"
#include "LXAnimation.h"
#include "LXProject.h"
#include "LXSettings.h"
#include "LXScript.h"
#include "LXLogger.h"
#include "LXMaterial.h"
#include "LXShader.h"
#include "LXTexture.h"
#include "LXProceduralTexture.h"
#include "LXDirectory.h"
#include "LXImporter.h"
#include "LXAssetMesh.h"
#include "LXMesh.h"
#include "LXConsoleManager.h"
#include "LXGraphTemplate.h"
#include "LXMemory.h" // --- Must be the last included ---

#define LX_DEFAULT_MATERIAL L"Materials/M_Default.smat"

//------------------------------------------------------------------------------------------------------
// Console commands
//------------------------------------------------------------------------------------------------------

LXConsoleCommand1S CCCreateNewMaterial(L"CreateNewMaterial", [](const LXString& inMaterialName)
{
	LXProject* Project = GetCore().GetProject();
	if (!Project)
	{
		LogW(Core, L"No project");
	}
	else
	{
		Project->GetAssetManager().CreateNewMaterial(inMaterialName, LX_DEFAULT_MATERIAL_FOLDER);
	}
});

LXConsoleCommand1S CCCreateNewTexture(L"CreateNewTexture", [](const LXString& inTextureName)
{
	LXProject* Project = GetCore().GetProject();
	if (!Project)
	{
		LogW(Core, L"No project");
	}
	else
	{
		Project->GetAssetManager().CreateNewTexture(inTextureName, LX_DEFAULT_TEXTURE_FOLDER);
	}
});

LXConsoleCommand1S CCCreateNewShader(L"CreateNewShader", [](const LXString& inShaderName)
{
	LXProject* Project = GetCore().GetProject();
	if (!Project)
	{
		LogW(Core, L"No project");
	}
	else
	{
		Project->GetAssetManager().CreateNewShader(inShaderName, LX_DEFAULT_SHADER_FOLDER);
	}
});

LXConsoleCommand1S CCCreateNewAnimation(L"CreateNewAnimation", [](const LXString& inAnimationName)
{
	LXProject* Project = GetCore().GetProject();
	if (!Project)
	{
		LogW(Core, L"No project");
	}
	else
	{
		Project->GetAssetManager().CreateNewAnimation(inAnimationName, LX_DEFAULT_ANIMATION_FOLDER);
	}
});

//------------------------------------------------------------------------------------------------------

const bool ForceLowercase = false;

LXAssetManager::LXAssetManager(LXProject* Project) :_pDocument(Project)
{
	_graphMaterialTemplate = make_unique<LXGraphTemplate>();
	VRF(_graphMaterialTemplate->LoadWithMSXML(GetSettings().GetCoreFolder() + L"/GraphMaterialTempale.xml"));
				
	_ListAssetExtentions.push_back(LX_MATERIAL_EXT);
	_ListAssetExtentions.push_back(LX_TEXTURE_EXT);
	_ListAssetExtentions.push_back(LX_SHADER_EXT);
	_ListAssetExtentions.push_back(LX_ANIMATION_EXT);
	
	// Engine Asset
	LoadFromFolder(GetSettings().GetDataFolder(), EResourceOwner::LXResourceOwner_Engine);
	
	// Project Asset
	if (Project->GetSeetronProject())
	{
		LoadFromFolder(Project->GetAssetFolder(), EResourceOwner::LXResourceOwner_Project);
	}
}

LXAssetManager::~LXAssetManager()
{
	for (auto It : _MapAssets)
		delete It.second;
}

LXMaterial*	LXAssetManager::GetDefaultMaterial()
{
	LXMaterial* Material = GetMaterial(LX_DEFAULT_MATERIAL);
	CHK(Material);
	return Material;
}

LXAsset* LXAssetManager::GetAsset(const LXString& Name) const
{
	LXString key = Name;
	key.MakeLower();

	auto It = _MapAssets.find(key);
	if (It != _MapAssets.end())
	{
		LXAsset* Resource = It->second;
		if (Resource->State == LXAsset::EResourceState::LXResourceState_Unloaded)
		{
			Resource->Load();
			LogI(AssetManager, L"Loaded %s", Resource->GetFilepath().GetBuffer());
		}

		if (Resource->State == LXAsset::EResourceState::LXResourceState_Loaded)
		{
			return Resource;
		}
		else
		{
			return nullptr;
		}
	}
	else
	{
		LogE(AssetManager, L"Resource %s not exists in ResourceManager", Name.GetBuffer());
		return nullptr;
	}
}

template <typename T>
T LXAssetManager::GetResourceT(const LXString& Name) const
{
	return dynamic_cast<T>(GetAsset(Name));
}

LXMaterial* LXAssetManager::GetMaterial(const LXString& strFilename) const
{
	return GetResourceT<LXMaterial*>(strFilename);
}

LXShader* LXAssetManager::GetShader(const LXString& strFilename) const
{
	return GetResourceT<LXShader*>(strFilename);
}

LXTexture* LXAssetManager::GetTexture(const LXString& strFilename) const
{
	return GetResourceT<LXTexture*>(strFilename);
}

LXAssetMesh* LXAssetManager::GetAssetMesh(const LXString& strFilename) const
{
	return GetResourceT<LXAssetMesh*>(strFilename);
}

LXScript* LXAssetManager::GetScript(const LXString& strFilename) const
{
	return GetResourceT<LXScript*>(strFilename);
}

LXGraphTexture* LXAssetManager::GetGraphTexture(const LXString& strFilename) const
{
	//return GetResourceT<LXGraphTexture*>(strFilename);
	return nullptr;
}

LXMaterial* LXAssetManager::CreateNewMaterial(const LXString& MaterialName, const LXString& RelativeAssetFolder)
{
	LXFilepath AssetFolderpath = GetCore().GetProject()->GetAssetFolder();
	LXFilepath MaterialFilepath;
	
	if (MaterialName.IsEmpty())
	{
		int i = 0;
		while (1)
		{
			LXString GeneratedMaterialName = L"M_Material" + LXString::Number(i);
			MaterialFilepath = AssetFolderpath + RelativeAssetFolder + GeneratedMaterialName + L"." + LX_MATERIAL_EXT;
			if (!MaterialFilepath.IsFileExist())
				break;
			i++;
		}
	}
	else
	{
		MaterialFilepath = AssetFolderpath + RelativeAssetFolder + MaterialName + L"." + LX_MATERIAL_EXT;
		if (MaterialFilepath.IsFileExist())
		{
			LogW(ResourceManager, L"Material %s already exist (%S).", MaterialName.GetBuffer(), MaterialFilepath.GetBuffer());
			return GetMaterial(RelativeAssetFolder + MaterialName + L"." + LX_MATERIAL_EXT);
		}
	}
	

	LXMaterial* Material = new LXMaterial();
	Material->Owner = EResourceOwner::LXResourceOwner_Project;
	Material->SetFilepath(MaterialFilepath);
	Material->State = LXAsset::EResourceState::LXResourceState_Loaded;
	Material->Save();
	
	LXFilepath RelativeAssetFilepath = AssetFolderpath.GetRelativeFilepath(MaterialFilepath);
	RelativeAssetFilepath.MakeLower();
	_MapAssets[RelativeAssetFilepath] = Material;

	LogI(ResourceManager, L"Material %s created (%s).", MaterialName.GetBuffer(), MaterialFilepath.GetBuffer());
	return Material;
}

LXShader* LXAssetManager::CreateNewShader(const LXString& ShaderName, const LXString& RelativeAssetFolder)
{
	LXFilepath AssetFolderpath = GetCore().GetProject()->GetAssetFolder();
	LXFilepath ShaderFilepath;

	if (ShaderName.IsEmpty())
	{
		int i = 0;
		while (1)
		{
			LXString GeneratedShaderName = L"M_Shader" + LXString::Number(i);
			ShaderFilepath = AssetFolderpath + RelativeAssetFolder + GeneratedShaderName + L"." + LX_SHADER_EXT;
			if (!ShaderFilepath.IsFileExist())
				break;
			i++;
		}
	}
	else
	{
		ShaderFilepath = AssetFolderpath + RelativeAssetFolder + ShaderName + L"." + LX_SHADER_EXT;
		if (ShaderFilepath.IsFileExist())
		{
			LogW(ResourceManager, L"Shader %s already exist (%S).", ShaderName.GetBuffer(), ShaderFilepath.GetBuffer());
			return GetShader(RelativeAssetFolder + ShaderName + L"." + LX_SHADER_EXT);
		}
	}


	LXShader* Shader = new LXShader();
	Shader->Owner = EResourceOwner::LXResourceOwner_Project;
	Shader->SetFilepath(ShaderFilepath);
	Shader->State = LXAsset::EResourceState::LXResourceState_Loaded;
	Shader->SaveDefault();

	LXFilepath RelativeAssetFilepath = AssetFolderpath.GetRelativeFilepath(ShaderFilepath);
	RelativeAssetFilepath.MakeLower();
	_MapAssets[RelativeAssetFilepath] = Shader;

	LogI(ResourceManager, L"Shader %s created (%s).", ShaderName.GetBuffer(), ShaderFilepath.GetBuffer());
	return Shader;
}

LXTexture* LXAssetManager::CreateNewTexture(const LXString& MaterialName, const LXString& RelativeAssetFolder)
{
	LXFilepath AssetFolderpath = GetCore().GetProject()->GetAssetFolder();
	LXFilepath TextureFilepath;

	if (MaterialName.IsEmpty())
	{
		int i = 0;
		while (1)
		{
			LXString GeneratedTextureName = L"T_Texture" + LXString::Number(i);
			TextureFilepath = AssetFolderpath + RelativeAssetFolder + GeneratedTextureName + L"." + LX_TEXTURE_EXT;
			if (!TextureFilepath.IsFileExist())
				break;
			i++;
		}
	}
	else
	{
		TextureFilepath = AssetFolderpath + RelativeAssetFolder + MaterialName + L"." + LX_TEXTURE_EXT;
		if (TextureFilepath.IsFileExist())
		{
			LogW(ResourceManager, L"Texture %s already exist (%S).", MaterialName.GetBuffer(), TextureFilepath.GetBuffer());
			return false;
		}
	}


	LXTexture* Texture = new LXTexture();
	Texture->Owner = EResourceOwner::LXResourceOwner_Project;
	Texture->SetFilepath(TextureFilepath);
	Texture->State = LXAsset::EResourceState::LXResourceState_Loaded;
	Texture->TextureSource = ETextureSource::TextureSourceDynamic;
	Texture->Save();

	LXFilepath RelativeAssetFilepath = AssetFolderpath.GetRelativeFilepath(TextureFilepath);
	RelativeAssetFilepath.MakeLower();
	_MapAssets[RelativeAssetFilepath] = Texture;

	LogW(ResourceManager, L"Texture %s created (%S).", MaterialName.GetBuffer(), TextureFilepath.GetBuffer());
	return Texture;
}


LXAnimation* LXAssetManager::CreateNewAnimation(const LXString& AnimationName, const LXString& RelativeAssetFolder)
{
	LXFilepath AssetFolderpath = GetCore().GetProject()->GetAssetFolder();
	LXFilepath AnimationFilepath;

	if (AnimationName.IsEmpty())
	{
		int i = 0;
		while (1)
		{
			LXString GeneratedAnimationName = L"M_Animation" + LXString::Number(i);
			AnimationFilepath = AssetFolderpath + RelativeAssetFolder + GeneratedAnimationName + L"." + LX_ANIMATION_EXT;
			if (!AnimationFilepath.IsFileExist())
				break;
			i++;
		}
	}
	else
	{
		AnimationFilepath = AssetFolderpath + RelativeAssetFolder + AnimationName + L"." + LX_ANIMATION_EXT;
		if (AnimationFilepath.IsFileExist())
		{
			LogW(ResourceManager, L"Animation %s already exist (%S).", AnimationName.GetBuffer(), AnimationFilepath.GetBuffer());
			return GetResourceT<LXAnimation*>(RelativeAssetFolder + AnimationName + L"." + LX_ANIMATION_EXT);
		}
	}


	LXAnimation* Animation = new LXAnimation();
	Animation->Owner = EResourceOwner::LXResourceOwner_Project;
	Animation->SetFilepath(AnimationFilepath);
	Animation->State = LXAsset::EResourceState::LXResourceState_Loaded;
	Animation->Save();

	LXFilepath RelativeAssetFilepath = AssetFolderpath.GetRelativeFilepath(AnimationFilepath);
	RelativeAssetFilepath.MakeLower();
	_MapAssets[RelativeAssetFilepath] = Animation;

	LogI(ResourceManager, L"Animation %s created (%s).", AnimationName.GetBuffer(), AnimationFilepath.GetBuffer());
	return Animation;
}

LXAsset* LXAssetManager::Import(const LXFilepath& Filepath, const LXString& RelativeAssetFolder, bool Transient)
{
	LXString Extension = Filepath.GetExtension().MakeLower();

	LXFilepath AssetFolderpath = GetCore().GetProject()->GetAssetFolder();
	LXFilepath RelativeSourceFilepath = AssetFolderpath.GetRelativeFilepath(Filepath);

	bool FileAlreadyInAssetFolder = false;
	
	if (RelativeSourceFilepath.size() > 0)
	{
		FileAlreadyInAssetFolder = true;
	}
	else
	{
		AssetFolderpath = GetSettings().GetDataFolder();
		RelativeSourceFilepath = AssetFolderpath.GetRelativeFilepath(Filepath);
		if (RelativeSourceFilepath.size() > 0)
		{
			FileAlreadyInAssetFolder = true;
		}
	}
	
	//
	// Textures
	//

	if ((Extension == L"jpg") || 
		(Extension == L"pbm") || 
		(Extension == L"png") || 
		(Extension == L"hdr") ||
		(Extension == L"tga"))
	{

		LXFilepath FinalFilepath = Filepath;

		// Copy file to folder asset
		if (!FileAlreadyInAssetFolder)
		{
			FinalFilepath = AssetFolderpath + RelativeAssetFolder + Filepath.GetFilename();
			if (!CopyFile(Filepath, FinalFilepath, FALSE))
			{
				DWORD LastError = GetLastError();
				LogE(LXAssetManager, L"Unable to copy file %s to %s. Windows LastError: %i", Filepath.GetBuffer(), FinalFilepath.GetBuffer(), LastError);
			}
			RelativeSourceFilepath = AssetFolderpath.GetRelativeFilepath(FinalFilepath);
		}
		
		LXTexture* pTexture = new LXTexture();
		pTexture->SetPersistent(!Transient);
		LXFilepath AssetFilepath = FinalFilepath;
		AssetFilepath.RemoveExtension();
		AssetFilepath = AssetFilepath + L"." + LX_TEXTURE_EXT;
		
		pTexture->Owner = EResourceOwner::LXResourceOwner_Project;
		pTexture->SetFilepath(AssetFilepath);
		pTexture->SetSource(RelativeSourceFilepath);
		pTexture->LoadSource();

		// As we create a new asset we can consider it as loaded.
		pTexture->State = LXAsset::EResourceState::LXResourceState_Loaded;
		pTexture->Save();

		LXFilepath RelativeAssetFilepath = AssetFolderpath.GetRelativeFilepath(AssetFilepath);
		RelativeAssetFilepath.MakeLower();
		_MapAssets[RelativeAssetFilepath] = pTexture;

		LogI(AssetManager, L"Successful import %s", Filepath.GetBuffer());
		return pTexture;
	}
	else if (LXImporter* Importer = GetCore().GetPlugin(_pDocument, Extension))
	{
		LXAssetMesh* AssetMesh = nullptr;
		LXMesh* Mesh = Importer->Load(Filepath);
		
		if (Mesh)
		{
			// Path
			LXFilepath AssetFilepath = AssetFolderpath + RelativeAssetFolder + Filepath.GetFilename();
			AssetFilepath.RemoveExtension();
			AssetFilepath = AssetFilepath + L"." + LX_MESH_EXT;
			LXFilepath RelativeAssetFilepath = AssetFolderpath.GetRelativeFilepath(AssetFilepath);
			
			// Resource
			AssetMesh = new LXAssetMesh(Mesh);
			AssetMesh->SetPersistent(!Transient);
			AssetMesh->Owner = EResourceOwner::LXResourceOwner_Project;
			AssetMesh->SetFilepath(AssetFilepath);
			//MiniScene->SetSource(Filepath);

			// As we create a new asset we can consider it as loaded.
			AssetMesh->State = LXAsset::EResourceState::LXResourceState_Loaded;
			AssetMesh->Save();

			_MapAssets[RelativeAssetFilepath] = AssetMesh;

			LogI(AssetManager, L"Successful import %s", Filepath.GetBuffer());
		}
		else
		{
			LogE(AssetManager, L"Failed to import import %s", Filepath.GetBuffer());
		}
		
		delete Importer;
		return AssetMesh;
	}
	
	LogE(AssetManager, L"File not supported: %s", Filepath.GetBuffer());
	return nullptr;
}

void LXAssetManager::Add(LXAsset* Resource)
{
	CHK(Resource);

	if (Resource->GetFilepath().IsEmpty())
	{
		LXFilepath Filepath = GetSettings().GetDataFolder() + Resource->GetName() + L"." + Resource->GetFileExtension();
		Resource->SetFilepath(Filepath);
	}

	LXString key = Resource->GetName();
	key.MakeLower();
	auto It = _MapAssets.find(key);
	if (It != _MapAssets.end())
	{
		CHK(0); 
		return;
	}

	_MapAssets[key] = Resource;
}

void LXAssetManager::LoadFromFolder(const LXFilepath& Folderpath, EResourceOwner ResourceOwner)
{
	LXFilepath AssetFolderpath = Folderpath;
	LXFilepath Path = AssetFolderpath + "*";
	//Path += "*";
	LXDirectory dir(Path);
	for (auto& FileInfo : dir.GetListFileNames())
	{
		LXFilepath Filepath(FileInfo.FullFileName);
		LXFilepath RelativeFilepath = AssetFolderpath.GetRelativeFilepath(Filepath);
		LXString Extension = Filepath.GetExtension().MakeLower();
		
		LXString AssetKey = BuildKey(ResourceOwner, RelativeFilepath);
		AssetKey.MakeLower();

		LXAsset* Asset = FindAsset(AssetKey);
		if (Asset)
		{
			LogE(AssetManager, L"Asset %s already exist", AssetKey.GetBuffer())
			continue;
		}
		
		if (Extension == LX_MATERIAL_EXT)
		{
			LXMaterial* pMaterial = new LXMaterial;
			pMaterial->SetFilepath(Filepath);
			pMaterial->Owner = ResourceOwner;
			_MapAssets[AssetKey] = pMaterial;
			LogI(AssetManager,L"Added %s", Filepath.GetBuffer());
		}
		else if (Extension == LX_TEXTURE_EXT)
		{
			LXTexture* pTexture = new LXTexture();
			pTexture->SetFilepath(Filepath);
			pTexture->Owner = ResourceOwner;
			_MapAssets[AssetKey] = pTexture;
			LogI(AssetManager,L"Added %s", Filepath.GetBuffer());
		}
		else if (Extension == LX_MESH_EXT)
		{
			LXAssetMesh* pMesh = new LXAssetMesh();
			pMesh->SetFilepath(Filepath);
			pMesh->Owner = ResourceOwner;
			_MapAssets[AssetKey] = pMesh;
			LogI(AssetManager, L"Added %s", Filepath.GetBuffer());
		}
		else if (Extension == LX_SHADER_EXT)
		{
			LXShader* Shader = new LXShader();
			Shader->SetFilepath(Filepath);
			Shader->Owner = ResourceOwner;
			_MapAssets[AssetKey] = Shader;
			LogI(AssetManager, L"Added %s", Filepath.GetBuffer());
		}
		else if (Extension == LX_ANIMATION_EXT)
		{
			LXAnimation* Shader = new LXAnimation();
			Shader->SetFilepath(Filepath);
			Shader->Owner = ResourceOwner;
			_MapAssets[AssetKey] = Shader;
			LogI(AssetManager, L"Added %s", Filepath.GetBuffer());
		}
				

#ifdef LX_USE_RAW_TEXTURES_AS_ASSETS
		else if ((Extension == L"jpg") || 
				 (Extension == L"pbm") || 
				 (Extension == L"png") || 
				 (Extension == L"tga"))
		{
			LXTexture* pTexture = new LXTexture();
			pTexture->SetSource(RelativeFilepath);
			pTexture->Owner = ResourceOwner;
			_MapAssets[AssetKey] = pTexture;
			LogI(AssetManager,L"Added %s", Filepath.GetBuffer());
		}
#endif 	
		else
		{
			//LogE(AssetManager, L"Unknow resource file format %s", Filepath.GetBuffer());
		}
	}
}

LXMaterial* LXAssetManager::CreateEngineMaterial(const wchar_t* szName)
{
	LXMaterial* Material = new LXMaterial();
	Material->SetName(szName);
	Material->State = LXAsset::EResourceState::LXResourceState_NotAFile;
	Material->Owner = EResourceOwner::LXResourceOwner_Engine;
	Add(Material);
	return Material;
}

template<typename T>
void LXAssetManager::GetAssets(list<T>& listAssets) const
{
	for (auto It : _MapAssets)
	{
		if (T resource = dynamic_cast<T>(It.second))
			listAssets.push_back(resource);
	}
}

void LXAssetManager::GetAssetsOfType(list<LXAsset*>& listAssets, LXAsset* Asset) const
{
	if (dynamic_cast<LXMaterial*>(Asset))
	{
		GetMaterials((list<LXMaterial*>&)listAssets);
	}
	else if (dynamic_cast<LXTexture*>(Asset))
	{
		GetTextures((list<LXTexture*>&)listAssets);
	}
	else if (dynamic_cast<LXShader*>(Asset))
	{
		GetShaders((list<LXShader*>&)listAssets);
	}
	else if (dynamic_cast<LXAssetMesh*>(Asset))
	{
		GetMeshes((list<LXAssetMesh*>&)listAssets);
	}
	else
	{
		if (Asset != nullptr)
		{
			LogE(AssetManager, L"Unknown asset type. Return all of them");
		}

		GetAssets(listAssets);
	}
}

void LXAssetManager::OnAssetRenamed(const LXString& Path, const LXString& OldName, const LXString& NewName, EResourceOwner ResourceOwner)
{
	LXFilepath OldFilepath = Path + L"/" + OldName;
	LXFilepath NewFilepath = Path + L"/" + NewName;

	CHK(NewFilepath.IsFileExist());

	LXFilepath AssetFolderpath;
	
	if (ResourceOwner == EResourceOwner::LXResourceOwner_Project)
	{
		LXProject* Project = GetCore().GetProject();
		AssetFolderpath = Project->GetAssetFolder();
	}
	else if (ResourceOwner == EResourceOwner::LXResourceOwner_Engine)
	{
		AssetFolderpath = GetSettings().GetDataFolder();
	}

	LXFilepath OldRelativeFilepath = AssetFolderpath.GetRelativeFilepath(OldFilepath);
	LXFilepath NewRelativeFilepath = AssetFolderpath.GetRelativeFilepath(NewFilepath);
	
	LXString OldAssetKey = BuildKey(ResourceOwner, OldRelativeFilepath);
	LXString NewAssetKey = BuildKey(ResourceOwner, NewRelativeFilepath);
	
	LXAsset* Asset = FindAsset(OldAssetKey);
	CHK(Asset);

	_MapAssets.erase(OldAssetKey);
	_MapAssets[NewAssetKey] = Asset;
}

LXAsset* LXAssetManager::FindAsset(const LXString& RelativeFilepath) const
{
	LXString LC = RelativeFilepath;
	LC.MakeLower();
	auto It = _MapAssets.find(LC);
	if (It != _MapAssets.end())
		return (*It).second;
	else 
		return nullptr;
}

void LXAssetManager::GetTextures(list<LXTexture*>& listTextures)const
{
	GetAssets<LXTexture*>(listTextures);
}

void LXAssetManager::GetScripts(list<LXScript*>& listScripts)const
{
	GetAssets<LXScript*>(listScripts);
}

void LXAssetManager::GetMaterials(list<LXMaterial*>& listMaterials)const
{
	GetAssets<LXMaterial*>(listMaterials);
}

void LXAssetManager::GetShaders(list<LXShader*>& listShaders)const
{
	GetAssets<LXShader*>(listShaders);
}

void LXAssetManager::GetMeshes(list<LXAssetMesh*>& listMeshes)const
{
	GetAssets<LXAssetMesh*>(listMeshes);
}

void LXAssetManager::GetAssets(list<LXAsset*>& listResources) const
{
	for (auto It : _MapAssets)
		listResources.push_back(It.second);
}

LXString LXAssetManager::BuildKey(EResourceOwner ResourceOwner, const LXString& RelativeFilepath)
{
	return RelativeFilepath;

	if (ResourceOwner == EResourceOwner::LXResourceOwner_Engine)
	{
		return L"engine:" + RelativeFilepath;
	}
	else if (ResourceOwner == EResourceOwner::LXResourceOwner_Project)
	{
		return L"project:" + RelativeFilepath;
	}
	else
	{
		CHK(0);
		return L"";
	}
}
