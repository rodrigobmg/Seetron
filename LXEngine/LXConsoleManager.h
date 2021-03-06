//------------------------------------------------------------------------------------------------------
//
// This is a part of Seetron Engine
//
// Copyright (c) 2018 Nicolas Arques. All rights reserved.
//
//------------------------------------------------------------------------------------------------------

#pragma once

#include "LXObject.h"

class LXConsoleCommand;

class LXCORE_API LXConsoleManager : public LXObject
{
public:

	LXConsoleManager();
	virtual ~LXConsoleManager();

	static void DeleteSingleton();

	bool TryToExecute(const LXString& CommandLine);
	void GetNearestCommand(const LXString& Str, vector<LXString>& ListSuggestion);
	
	void AddCommand(LXConsoleCommand* command);
	void RemoveCommand(LXConsoleCommand* command);

	list<LXConsoleCommand*> ListCommands;
};

LXCORE_API LXConsoleManager& GetConsoleManager();

class LXCORE_API LXConsoleCommand : public LXObject
{

public:

	LXConsoleCommand(const LXString& name);
	virtual ~LXConsoleCommand();

	virtual void Execute(const vector<LXString>& Arguments) = 0;

public:
	
	LXString Name;
	uint MenuID = -1;
};

//------------------------------------------------------------------------------------------------------
// ConsoleCommand based on a existing variable 
//------------------------------------------------------------------------------------------------------

template<typename T>
class LXCORE_API LXConsoleCommandT : public LXConsoleCommand
{
public:

	LXConsoleCommandT(const LXString& InName, T* inVar);
	LXConsoleCommandT(const LXString& InFile, const LXString& InSection, const LXString& InName, const LXString& InDefault);
	virtual ~LXConsoleCommandT();

	const T& GetValue();
	void Execute(const vector<LXString>& Arguments) override;

private:

	void SetValue(const T& value) { *Var = value; }
	void SetValueFromString(const LXString& Value);
	void ReadValueFromPrivateProfile();

private:

	T* Var = nullptr;

	struct LXPrivateProfile
	{
		LXString File;
		LXString Section;
		LXString Name;
		LXString DefaultValue;

	}* _privateProfile = nullptr;
	
};

//------------------------------------------------------------------------------------------------------
// ConsoleCommand based on method V2.0
//------------------------------------------------------------------------------------------------------

template<typename FunctionSignature>
class LXCORE_API LXConsoleCommandCall2 : public LXConsoleCommand
{

public:

	LXConsoleCommandCall2(const LXString& InName, std::function<FunctionSignature> InFunction);
	void Execute(const vector<LXString>& Arguments) override;

	std::function<FunctionSignature> Function;
};

typedef LXConsoleCommandCall2<void()> LXConsoleCommandNoArg;
typedef LXConsoleCommandCall2<void(const LXString&)> LXConsoleCommand1S;
typedef LXConsoleCommandCall2<void(const LXString&, const LXString&)> LXConsoleCommand2S;
typedef LXConsoleCommandCall2<void(const LXString&, const LXString&, const LXString&)> LXConsoleCommand3S;
