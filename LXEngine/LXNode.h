//------------------------------------------------------------------------------------------------------
//
// This is a part of Seetron Engine
//
// Copyright (c) 2018 Nicolas Arques. All rights reserved.
//
//------------------------------------------------------------------------------------------------------

#pragma once

#include "LXSmartObject.h"

class LXConnector;
class LXNodeTemplate;
class LXGraph;
class LXConnectorTemplate;

enum class ENodeType
{
	Undefined,
	Variable,
	Function
};

class LXCORE_API LXNode : public LXSmartObject
{

public:

	LXNode(LXGraph* graph);
	LXNode(LXGraph* graph, const LXNodeTemplate* nodeTemplate);
	virtual ~LXNode();

	int GetOuputConnectorIndex(const LXConnector* connector);

	void GetNewListItemChoices(const LXPropertyListSmartObjects* property, list<LXString>& outStrings) override;

	LXSmartObject* AddItemToPropertyList(const LXPropertyListSmartObjects* property, const LXString& id) override;

	const LXConnector* GetInputConnector(const LXString& connectorName) const;

	const LXConnector* GetOutputConnector(const LXString& connectorName) const;

private:

	void DefineProperties();
	void OnLoaded() override;
	
	// Add missing connectors
	void Update(const LXString& templateID);
	
public:

	class LXGraph* Graph = nullptr;

	LXString TemplateID;
	vec2f Position = vec2f(0.f, 0.f);
	const LXNodeTemplate* NodeTemplate = nullptr;
	bool Main = false;
	bool EditableInputs = false;

	// Connector
	list<LXConnector*> Inputs;
	list<LXConnector*> Outputs;


};

struct LXVariableTemplate
{
	LXString Name;
	LXString Declaration;
};

class LXCORE_API LXNodeTemplate : public LXSmartObject
{

public:

	LXNodeTemplate();
	~LXNodeTemplate();

	const LXConnectorTemplate* GetOutputConnectorTemplate(int index) const;
	
private:

	bool OnLoadChild(const TLoadContext& loadContext) override;
	
public:
		
	LXString Type;
	bool Main = false;
			
	// Connector template
	vector<LXConnectorTemplate*> Inputs;
	vector<LXConnectorTemplate*> Outputs;
	vector<LXVariableTemplate> Variables;

};