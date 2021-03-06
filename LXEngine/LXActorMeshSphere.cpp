//------------------------------------------------------------------------------------------------------
//
// This is a part of Seetron Engine
//
// Copyright (c) 2018 Nicolas Arques. All rights reserved.
//
//------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "LXActorMeshSphere.h"
#include "LXPrimitive.h"
#include "LXMesh.h"
#include "LXCore.h"
#include "LXPrimitiveFactory.h"
#include "LXMemory.h" // --- Must be the last included ---

LXActorMeshSphere::LXActorMeshSphere(LXProject* pDocument) :LXActorMesh(pDocument)
{
	SetName(L"Sphere");

	Mesh = new LXMesh(nullptr);

	LXProperty::SetCurrentGroup(L"Sphere");
	
	DefineProperty("Slices", &Slices, 1, 64);
	DefineProperty("Stacks", &Stacks, 1, 64);

	LXPropertyAssetPtr* pPropMaterial = DefinePropertyAsset(L"Material", LXPropertyID::PRIMITIVE_MATERIAL, (LXAsset**)&_Material);
	pPropMaterial->SetName(L"Material");
	pPropMaterial->SetLambdaOnChange([this](LXPropertyAssetPtr* PropertyAsset)
	{
		if (LXAsset* Asset = PropertyAsset->GetValue())
		{
			LXString Key = PropertyAsset->GetValue()->GetRelativeFilename();
			Mesh->SetMaterial(Key);
		}
	});
	
	const shared_ptr<LXPrimitive>& Primitive = GetPrimitiveFactory()->CreateSphere(50.0f);
	Primitive->SetPersistent(false);
	Primitive->ComputeTangents();
	AddPrimitive(Primitive);
}

LXActorMeshSphere::~LXActorMeshSphere()
{
	delete Mesh;
}

void LXActorMeshSphere::OnPropertyChanged(LXProperty* Property)
{
	__super::OnPropertyChanged(Property);

	if (Property->GetID() == LXPropertyID::PRIMITIVE_MATERIAL)
	{
		LXPropertyAssetPtr* PropertyAsset = static_cast<LXPropertyAssetPtr*>(Property);
		LXString Key = PropertyAsset->GetValue()->GetRelativeFilename();
		Mesh->SetMaterial(Key);
	}
}

//------------------------------------------------------------------------------------------------------

LXActorMeshCylinder::LXActorMeshCylinder(LXProject* pDocument) :LXActorMesh(pDocument)
{
	SetName(L"Cylinder");

	Mesh = new LXMesh(nullptr);

	LXProperty::SetCurrentGroup(L"Cylinder");

	DefineProperty("Slices", &Slices, 1, 64);
	DefineProperty("Stacks", &Stacks, 1, 64);

	LXPropertyAssetPtr* pPropMaterial = DefinePropertyAsset(L"Material", LXPropertyID::PRIMITIVE_MATERIAL, (LXAsset**)&_Material);
	pPropMaterial->SetName(L"Material");
	pPropMaterial->SetLambdaOnChange([this](LXPropertyAssetPtr* PropertyAsset)
	{
		if (LXAsset* Asset = PropertyAsset->GetValue())
		{
			LXString Key = PropertyAsset->GetValue()->GetRelativeFilename();
			Mesh->SetMaterial(Key);
		}
	});

	const shared_ptr<LXPrimitive>& Primitive = GetPrimitiveFactory()->CreateCylinder(50.0f, 100.0f);
	Primitive->SetPersistent(false);
	Primitive->ComputeTangents();
	AddPrimitive(Primitive);
}

LXActorMeshCylinder::~LXActorMeshCylinder()
{
	delete Mesh;
}

void LXActorMeshCylinder::OnPropertyChanged(LXProperty* Property)
{
	__super::OnPropertyChanged(Property);

	if (Property->GetID() == LXPropertyID::PRIMITIVE_MATERIAL)
	{
		LXPropertyAssetPtr* PropertyAsset = static_cast<LXPropertyAssetPtr*>(Property);
		LXString Key = PropertyAsset->GetValue()->GetRelativeFilename();
		Mesh->SetMaterial(Key);
	}
}

//------------------------------------------------------------------------------------------------------

LXActorMeshCone::LXActorMeshCone():LXActorMesh(GetCore().GetProject())
{
	SetName(L"Cone");

	Mesh = new LXMesh(nullptr);

	LXProperty::SetCurrentGroup(L"Cone");

	DefineProperty("Slices", &Slices, 1, 64);
	DefineProperty("Stacks", &Stacks, 1, 64);

	LXPropertyAssetPtr* pPropMaterial = DefinePropertyAsset(L"Material", LXPropertyID::PRIMITIVE_MATERIAL, (LXAsset**)&_Material);
	pPropMaterial->SetName(L"Material");
	pPropMaterial->SetLambdaOnChange([this](LXPropertyAssetPtr* PropertyAsset)
	{
		if (LXAsset* Asset = PropertyAsset->GetValue())
		{
			LXString Key = PropertyAsset->GetValue()->GetRelativeFilename();
			Mesh->SetMaterial(Key);
		}
	});

	const shared_ptr<LXPrimitive>& Primitive = GetPrimitiveFactory()->CreateCone(50.0f, 100.0f);
	Primitive->SetPersistent(false);
	Primitive->ComputeTangents();
	AddPrimitive(Primitive);
}

LXActorMeshCone::~LXActorMeshCone()
{
	delete Mesh;
}

void LXActorMeshCone::OnPropertyChanged(LXProperty* Property)
{
	__super::OnPropertyChanged(Property);

	if (Property->GetID() == LXPropertyID::PRIMITIVE_MATERIAL)
	{
		LXPropertyAssetPtr* PropertyAsset = static_cast<LXPropertyAssetPtr*>(Property);
		LXString Key = PropertyAsset->GetValue()->GetRelativeFilename();
		Mesh->SetMaterial(Key);
	}
}
